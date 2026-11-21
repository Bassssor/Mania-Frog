// Native animation filter for the official input-overlay source. No browser,
// websocket, external executable, or Qt dependency is involved.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <obs-module.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include "Fountain.hpp"
#include "../shared/LaughPlayback.hpp"
#include "../shared/ObsKeyLabels.hpp"
#include <util/platform.h>
#include <mmsystem.h>
#include <shellapi.h>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void) { return "Milk Frog native input-overlay animation"; }
MODULE_EXPORT const char *obs_module_name(void) { return "Milk Frog Input Overlay"; }

namespace {
#ifdef MILK_FROG_EASY
std::filesystem::path easyFolder;
std::atomic<bool> importQueued{false}, frontendReady{false};
using CurrentCollection = char *(*)();
using CurrentScene = obs_source_t *(*)();
CurrentCollection currentCollection=nullptr;
CurrentScene currentScene=nullptr;
void ProcessImports(void *);
void QueueImports() {
    std::error_code error;
    if(frontendReady && std::filesystem::is_directory(easyFolder/L"requests",error) &&
       !std::filesystem::is_empty(easyFolder/L"requests",error) && !importQueued.exchange(true))
        obs_queue_task(OBS_TASK_UI,ProcessImports,nullptr,false);
}
#endif
constexpr float Pi = 3.14159265358979323846f;
std::atomic<unsigned> heldKeys{0}, strikes[frog::MaxLanes];
std::thread hookThread;
std::atomic<DWORD> hookThreadId{0};
HHOOK keyboardHook = nullptr;
std::mutex playbackMutex;
frog::LaughPlayback playback;
frog::ObsBindings gameplayKeys=frog::DefaultObsBindings;
std::filesystem::path gameplayConfig;
void reloadLaughHotkey(bool force=false) {
    static std::filesystem::file_time_type previous=std::filesystem::file_time_type::min();
    const auto path=frog::LaughConfigPath();std::error_code error;
    auto timestamp=std::filesystem::last_write_time(path,error);
    if(error)timestamp=std::filesystem::file_time_type::min();
    if(!force && timestamp==previous)return;
    previous=timestamp;const auto chord=frog::ReadLaughHotkey(path);
    std::lock_guard<std::mutex> lock(playbackMutex);
    if(force || !(playback.hotkey==chord)) {
        playback.setHotkey(chord);playback.syncPressed();
        heldKeys=frog::ObsHeldMask(gameplayKeys,playback.pressed);
        blog(LOG_INFO,"[milk-frog] Laughter hotkey: %s",chord.text().c_str());
    }
}
frog::LaughPlayback playbackSnapshot() {
    std::lock_guard<std::mutex> lock(playbackMutex);
    playback.frame(frog::NowNs());return playback;
}
void reloadGameplayBindings() {
    static std::filesystem::path previousPath;
    static std::filesystem::file_time_type previousTime=std::filesystem::file_time_type::min();
    std::filesystem::path path;
    {std::lock_guard<std::mutex> lock(playbackMutex);path=gameplayConfig;}
    if(path.empty())return;
    std::error_code error;auto time=std::filesystem::last_write_time(path,error);
    if(error || (path==previousPath && time==previousTime))return;
    frog::ObsBindings candidate{};
    if(!frog::ReadObsBindings(path,candidate))return; // Keep the last valid mapping.
    previousPath=path;previousTime=time;
    std::lock_guard<std::mutex> lock(playbackMutex);
    if(gameplayKeys==candidate)return;
    gameplayKeys=candidate;playback.syncPressed();
    const unsigned mask=frog::ObsHeldMask(gameplayKeys,playback.pressed);heldKeys=mask;
    if(mask)playback.stop();
    blog(LOG_INFO,"[milk-frog] OBS gameplay keys: %u %u %u %u",candidate[0],candidate[1],candidate[2],candidate[3]);
}
LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM value) {
    if (code >= 0) {
        auto *key = reinterpret_cast<KBDLLHOOKSTRUCT *>(value);
        const bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
        const bool up=message==WM_KEYUP || message==WM_SYSKEYUP;
        if(down || up) {
            std::lock_guard<std::mutex> lock(playbackMutex);
            const unsigned old=heldKeys.load();
            bool bound=false;for(auto binding:gameplayKeys)bound|=frog::BindingMatches(binding,key->vkCode);
            playback.key(key->vkCode,down,bound,old!=0,frog::NowNs());
            const unsigned mask=frog::ObsHeldMask(gameplayKeys,playback.pressed);heldKeys=mask;
            for(unsigned i=0;i<gameplayKeys.size();++i)if((mask&~old)&(1u<<i))++strikes[i];
        }
    }
    return CallNextHookEx(keyboardHook, code, message, value);
}
const char *ParticleEffect = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform bool textured;
sampler_state linearSampler { Filter=Linear; AddressU=Clamp; AddressV=Clamp; };
struct Vertex { float4 pos:POSITION; float4 color:COLOR; float2 uv:TEXCOORD0; };
Vertex VS(Vertex v) { v.pos=mul(float4(v.pos.xyz,1.0),ViewProj); return v; }
float4 PS(Vertex v):TARGET {
    float a=v.color.a;
    if(textured) a*=image.Sample(linearSampler,v.uv).a;
    return float4(v.color.rgb*a,a);
}
technique Draw { pass { vertex_shader=VS(v); pixel_shader=PS(v); } }
)";

struct Vertex { float x,y,u,v; uint32_t color; };
struct GraphicsLock {
    GraphicsLock() {obs_enter_graphics();}
    ~GraphicsLock() {obs_leave_graphics();}
};
// OBS 33 removed the legacy gs_image_file API. The preset contains only PNGs;
// decode those with the same Windows GDI+ dependency used for the key labels.
struct ImageTexture {
    gs_texture_t *texture=nullptr;
    unsigned cx=0,cy=0;
    bool loaded=false;
    void free(){gs_texture_destroy(texture);texture=nullptr;cx=cy=0;loaded=false;}
    void load(const char *path) {
        free();frog::GdiSession session;
        Gdiplus::Bitmap bitmap(std::filesystem::u8path(path).c_str());
        if(bitmap.GetLastStatus()!=Gdiplus::Ok)return;
        cx=bitmap.GetWidth();cy=bitmap.GetHeight();
        if(!cx || !cy || cx>32768 || cy>32768)return;
        Gdiplus::Rect rect(0,0,cx,cy);Gdiplus::BitmapData bits{};
        if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&bits)!=Gdiplus::Ok)return;
        std::vector<uint8_t> pixels(size_t(cx)*cy*4);
        for(unsigned y=0;y<cy;++y){auto *row=static_cast<uint8_t*>(bits.Scan0)+ptrdiff_t(y)*bits.Stride;
            for(unsigned x=0;x<cx;++x){auto *p=&pixels[(size_t(y)*cx+x)*4];p[0]=row[x*4+2];p[1]=row[x*4+1];p[2]=row[x*4];p[3]=row[x*4+3];}}
        bitmap.UnlockBits(&bits);const uint8_t *data=pixels.data();
        texture=gs_texture_create(cx,cy,GS_RGBA,1,&data,0);loaded=texture!=nullptr;
    }
};
struct PartAtlas {
    ImageTexture image;
    unsigned x=0,y=0,width=0,height=0,columns=0,count=0;
    void free(){image.free();}
    bool load(obs_data_t *spec,const std::filesystem::path& directory) {
        free();if(!spec)return false;
        x=unsigned(obs_data_get_int(spec,"x"));y=unsigned(obs_data_get_int(spec,"y"));
        width=unsigned(obs_data_get_int(spec,"width"));height=unsigned(obs_data_get_int(spec,"height"));
        columns=unsigned(obs_data_get_int(spec,"columns"));count=unsigned(obs_data_get_int(spec,"count"));
        if(!width || !height || width>880 || height>880 || !columns || columns>4 || !count || count>32)return false;
        const auto path=(directory/std::filesystem::u8path(obs_data_get_string(spec,"file"))).u8string();
        image.load(path.c_str());
        const auto& file=image;
        return file.loaded && file.cx==(width+3)*columns+2 && file.cy==(height+3)*((count+columns-1)/columns)+2;
    }
    void draw(unsigned index) {
        auto *texture=image.texture;if(!texture || index>=count)return;
        auto *effect=obs_get_base_effect(OBS_EFFECT_DEFAULT);
        gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),texture);
        gs_matrix_push();gs_matrix_translate3f(float(x),float(y),0);
        while(gs_effect_loop(effect,"Draw"))gs_draw_sprite_subregion(texture,0,1+index%columns*(width+3),1+index/columns*(height+3),width,height);
        gs_matrix_pop();
    }
};
struct Filter {
    obs_source_t *source;
    bool standalone=false;
    std::string configPath;
    ImageTexture background,foreground;
    std::array<PartAtlas,4> parts{}; // Body, desk, hands, multiplicative arm shadows.
    bool separatedParts=false;
    gs_texture_t *halo = nullptr;
    gs_effect_t *effect = nullptr;
    gs_texrender_t *composed = nullptr;
    frog::LaughFrames laugh;
    gs_texture_t *laughTexture=nullptr,*labelsTexture=nullptr;
    frog::ObsBindings labelKeys=frog::DefaultObsBindings,renderedKeys{};
    bool dynamicLabels=false;
    unsigned atlasColumns=4;
    int laughFrame=-1;
    bool laughLoaded=false;
    std::array<gs_vertbuffer_t *,3> buffers{};
    frog::Fountain fountain;
    unsigned held = 0;
    std::array<unsigned,frog::MaxLanes> seen{};
    bool particlesEnabled = true, dirty = true, loaded = false;
    std::mutex mutex;
    std::vector<Vertex> glow, solid, trail;

    explicit Filter(obs_source_t *s,bool input=false): source(s),standalone(input) {
        for(unsigned i=0;i<frog::MaxLanes;++i) seen[i]=strikes[i].load();
        glow.reserve(2048*6+64); solid.reserve(2048*60+1600); trail.reserve(2048*2);
        obs_enter_graphics();
        char *error=nullptr;
        effect=gs_effect_create(ParticleEffect,"milk-frog-particles.effect",&error);
        if(error) { blog(LOG_ERROR,"[milk-frog] shader: %s",error); bfree(error); }
        composed=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
        std::array<uint8_t,64*64*4> pixels{};
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
            float dx=(x+.5f-32)/32,dy=(y+.5f-32)/32,r2=dx*dx+dy*dy;
            auto k=(y*64+x)*4; pixels[k]=pixels[k+1]=pixels[k+2]=255;
            pixels[k+3]=r2<1 ? uint8_t(std::exp(-r2*5)*(1-r2)*255) : 0;
        }
        const uint8_t *data=pixels.data(); halo=gs_texture_create(64,64,GS_RGBA,1,&data,0);
        // The libobs immediate-mode buffer is intended for small UI geometry.
        // Stream complete particle batches into bounded, reusable GPU buffers.
        size_t capacities[3]={2048*6+64,2048*60+1600,2048*2};
        for(int i=0;i<3;++i) {
            auto *vb=gs_vbdata_create();vb->num=capacities[i];
            vb->points=static_cast<vec3*>(bzalloc(vb->num*sizeof(vec3)));
            vb->colors=static_cast<uint32_t*>(bzalloc(vb->num*sizeof(uint32_t)));
            vb->num_tex=1;vb->tvarray=static_cast<gs_tvertarray*>(bzalloc(sizeof(gs_tvertarray)));
            vb->tvarray[0].width=2;vb->tvarray[0].array=bzalloc(vb->num*sizeof(vec2));
            buffers[i]=gs_vertexbuffer_create(vb,GS_DYNAMIC);
        }
        obs_leave_graphics();
    }
    ~Filter() {
        obs_enter_graphics();
        background.free();foreground.free();
        for(auto& part:parts)part.free();
        gs_texture_destroy(halo); gs_effect_destroy(effect); gs_texrender_destroy(composed);
        gs_texture_destroy(laughTexture);gs_texture_destroy(labelsTexture);
        for(auto *buffer:buffers)gs_vertexbuffer_destroy(buffer);
        obs_leave_graphics();
    }
    bool validAtlas(unsigned lanes) const {
        const auto& b=background;
        const auto& f=foreground;
        const unsigned rows=((1u<<lanes)+atlasColumns-1)/atlasColumns;
        return b.loaded && f.loaded && b.cx==atlasColumns*883+2 && b.cy==rows*883+2
            && f.cx==b.cx && f.cy==b.cy && effect && halo && composed
            && std::all_of(buffers.begin(),buffers.end(),[](auto *buffer){return buffer!=nullptr;});
    }
    void update(obs_data_t *settings) {
        // Settings may arrive on the UI/API thread while video rendering is
        // active. Acquire graphics before the state mutex in both paths.
        GraphicsLock graphics;
        std::lock_guard<std::mutex> lock(mutex);
        particlesEnabled=obs_data_get_bool(settings,"particles");
        if(!particlesEnabled) fountain=frog::Fountain();
        std::string path=obs_data_get_string(settings,"config_file");
        if(path==configPath) {dirty=true;return;}
        configPath=path; loaded=false;
        obs_data_t *config=obs_data_create_from_json_file(path.c_str());
        if(!config) { blog(LOG_ERROR,"[milk-frog] configuration unavailable"); return; }
        obs_data_t *spec=obs_data_get_obj(config,"milk_frog");
        const auto version=spec?obs_data_get_int(spec,"version"):0;
        if(!spec || (version!=1 && version!=2) || obs_data_get_int(spec,"tile_size")!=880 || obs_data_get_int(spec,"stride")!=883) {
            blog(LOG_ERROR,"[milk-frog] configuration is not a compatible mascot preset");
        } else {
            auto dir=std::filesystem::u8path(path).parent_path();
            separatedParts=version==2;
            atlasColumns=obs_data_get_int(spec,"atlas_columns")==8?8:4;
            dynamicLabels=obs_data_get_bool(spec,"dynamic_key_labels");
            if(dynamicLabels) {
                auto name=std::string(obs_data_get_string(spec,"keybindings_file"));if(name.empty())name="keybindings.json";
                std::lock_guard<std::mutex> inputLock(playbackMutex);gameplayConfig=dir/std::filesystem::u8path(name);
            }

            laughLoaded=laugh.load(dir/L"laugh-frames.mfa");
            if(!laughLoaded)blog(LOG_WARNING,"[milk-frog] Laughter video unavailable; normal inputs remain usable.");
            if(separatedParts) {
                auto *list=obs_data_get_obj(spec,"parts");const char* names[]={"body","desk","hand","shadow"};
                loaded=effect && halo && composed && std::all_of(buffers.begin(),buffers.end(),[](auto *buffer){return buffer!=nullptr;});
                for(unsigned i=0;i<parts.size();++i) {
                    auto *part=obs_data_get_obj(list,names[i]);loaded=parts[i].load(part,dir) && loaded;obs_data_release(part);
                }
                obs_data_release(list);
            } else {
            auto bg=(dir/std::filesystem::u8path(obs_data_get_string(spec,"background_atlas"))).u8string();
            auto fg=(dir/std::filesystem::u8path(obs_data_get_string(spec,"foreground_atlas"))).u8string();
            background.load(bg.c_str());foreground.load(fg.c_str());
            loaded=validAtlas(4);
            }
        }
        obs_data_release(spec); obs_data_release(config); dirty=true;
    }
    void tick(float seconds) {
        GraphicsLock graphics;
        std::lock_guard<std::mutex> lock(mutex);
        frog::ObsBindings currentKeys;unsigned mask=0,pending=0;
        {std::lock_guard<std::mutex> inputLock(playbackMutex);
         currentKeys=gameplayKeys;mask=heldKeys.load();
         for(unsigned i=0;i<frog::MaxLanes;++i) {unsigned now=strikes[i].load();if(now!=seen[i])pending|=1u<<i;seen[i]=now;}}
        if(dynamicLabels && currentKeys!=labelKeys) {
            if(!separatedParts && currentKeys.size()!=labelKeys.size()) {
                const auto dir=std::filesystem::u8path(configPath).parent_path();
                const auto bg=(dir/("background-"+std::to_string(currentKeys.size())+"k-atlas.png")).u8string();
                const auto fg=(dir/("foreground-"+std::to_string(currentKeys.size())+"k-atlas.png")).u8string();
                background.load(bg.c_str());foreground.load(fg.c_str());
                atlasColumns=currentKeys.size()==4?4:8;
                loaded=validAtlas(unsigned(currentKeys.size()));
            }
            labelKeys=currentKeys;fountain=frog::Fountain();dirty=true;
        }
        frog::LaneCount=unsigned(currentKeys.size());
        dirty=dirty || held!=mask || pending || fountain.active(); held=mask;
        auto state=playbackSnapshot();
        const int frame=laughLoaded?state.frame(frog::NowNs()):-1;
        if(frame!=laughFrame){laughFrame=frame;dirty=true;}
        if(particlesEnabled) {
            float remaining=std::clamp(seconds,0.f,.20f);bool first=true;
            do {float dt=std::min(1/120.f,remaining);fountain.advance(dt,held,first?pending:0);remaining-=dt;first=false;} while(remaining>1e-6f);
        }
    }
    static uint32_t color(int r,int g,int b,float a) {
        return uint32_t(r) | uint32_t(g)<<8 | uint32_t(b)<<16 | uint32_t(std::clamp(a,0.f,255.f))<<24;
    }
    void vertex(std::vector<Vertex>& batch,float x,float y,int r,int g,int b,float a,float u=0,float v=0) {
        batch.push_back({x-120,y-1,u,v,color(r,g,b,a)});
    }
    void glowQuad(float x,float y,float rx,float ry,int r,int g,int b,float a,float angle=0) {
        float c=std::cos(angle),s=std::sin(angle);
        float points[4][2]={{-rx,-ry},{rx,-ry},{rx,ry},{-rx,ry}};
        float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
        for(int i:{0,1,2,0,2,3})vertex(glow,x+points[i][0]*c-points[i][1]*s,y+points[i][0]*s+points[i][1]*c,r,g,b,a,uv[i][0],uv[i][1]);
    }
    void submit(const std::vector<Vertex>& batch,bool textured,gs_draw_mode mode) {
        if(batch.empty())return;
        auto *buffer=buffers[textured?0:mode==GS_LINES?2:1];
        auto *data=gs_vertexbuffer_get_data(buffer);
        if(!data || batch.size()>data->num)return;
        auto *uv=static_cast<vec2*>(data->tvarray[0].array);
        for(size_t i=0;i<batch.size();++i) {
            const auto& v=batch[i];vec3_set(&data->points[i],v.x,v.y,0);
            data->colors[i]=v.color;vec2_set(&uv[i],v.u,v.v);
        }
        auto view=*data;view.num=batch.size();
        gs_vertexbuffer_flush_direct(buffer,&view);
        gs_effect_set_bool(gs_effect_get_param_by_name(effect,"textured"),textured);
        gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),halo);
        while(gs_effect_loop(effect,"Draw")) {
            gs_load_vertexbuffer(buffer);gs_load_indexbuffer(nullptr);
            gs_draw(mode,0,static_cast<uint32_t>(batch.size()));
        }
    }
    void flush() {submit(glow,true,GS_TRIS);submit(solid,false,GS_TRIS);submit(trail,false,GS_LINES);}
    void surface() {
        glow.clear();solid.clear();trail.clear();
        for(unsigned i=0;i<labelKeys.size();++i) {
            auto p=fountain.centers[i];float c=fountain.charge[i];
            if(c>0) {glowQuad(p.x,p.y,38,16,255,200,65,c*(.94f+.06f*std::sin(fountain.time*9+i))*150,Pi/18);glowQuad(p.x,p.y,12,5,255,243,183,c*85,Pi/18);}
            if(fountain.strikeAge[i]<.30f) {
                float phase=fountain.strikeAge[i]/.30f,radius=7+phase*37,a=(1-phase)*155;
                for(int j=0;j<64;++j) {
                    frog::Point points[4];
                    for(int k=0;k<4;++k) {float angle=(j+k/2)*Pi/32,r=radius+(k%2)*1.4f,dx=std::cos(angle)*r,dy=std::sin(angle)*r*.37f;points[k]={p.x+dx-dy*.5f,p.y+dx*.16f+dy};}
                    for(int k:{0,1,2,2,1,3})vertex(solid,points[k].x,points[k].y,255,231,132,a);
                }
            }
        }
        flush();
    }
    void particles(int depth) {
        glow.clear();solid.clear();trail.clear();
        for(const auto& p:fountain.particles) {
            if((depth<0 && p.y>=0)||(depth>0 && p.y<0))continue;
            float x=p.anchor.x+p.x-p.y*.58f,y=p.anchor.y+p.x*.16f+p.y*.85f-p.z;
            float scale=std::clamp(1+p.y*.008f,.70f,1.30f),radius=p.radius*scale;
            float release=std::max(0.f,1-p.releaseAge/.40f),fade=std::pow(1-p.age/p.life,.85f)*release*release*(3-2*release);
            float a=std::min(255.f,240*fade*std::min(1.f,p.age*100+.2f)*scale),r=radius*(.7f+fade*.3f);
            glowQuad(x,y,radius*4.5f,radius*4.5f,255,193,48,a*.76f);
            for(int i=0;i<12;++i) {
                vertex(solid,x,y,255,244,182,a);
                vertex(solid,x+std::sin(i*Pi/6)*r,y-std::cos(i*Pi/6)*r,255,244,182,a);
                vertex(solid,x+std::sin((i+1)*Pi/6)*r,y-std::cos((i+1)*Pi/6)*r,255,244,182,a);
            }
            if(p.star) {
                float r=radius*(1.5f+fade),w=radius*.25f;float points[8][2]={{0,-r},{w,-w},{r,0},{w,w},{0,r},{-w,w},{-r,0},{-w,-w}};
                for(int i=0;i<8;++i) {vertex(solid,x,y,255,251,224,a);vertex(solid,x+points[i][0],y+points[i][1],255,251,224,a);vertex(solid,x+points[(i+1)%8][0],y+points[(i+1)%8][1],255,251,224,a);}
            } else if(p.age>.018f) {
                float dt=p.jet?.065f:.035f;
                vertex(trail,x,y,255,224,110,a*.5f);vertex(trail,x-(p.vx-p.vy*.58f)*dt,y-(p.vx*.16f+p.vy*.85f-p.vz)*dt,255,173,32,0);
            }
        }
        flush();
    }
    void sprite(gs_texture_t *texture) {
        auto *spriteEffect=obs_get_base_effect(OBS_EFFECT_DEFAULT);
        gs_effect_set_texture(gs_effect_get_param_by_name(spriteEffect,"image"),texture);
        while(gs_effect_loop(spriteEffect,"Draw"))gs_draw_sprite_subregion(texture,0,1+held%atlasColumns*883,1+held/atlasColumns*883,880,880);
    }
    void render() {
        std::lock_guard<std::mutex> lock(mutex);
        auto *parent=standalone?nullptr:obs_filter_get_parent(source);
        if(!loaded || (!standalone && (!parent || std::string(obs_source_get_id(parent))!="input-overlay"))) {
            if(parent)obs_source_skip_video_filter(source);return;
        }
        if(dirty) {
            if(dynamicLabels && (!labelsTexture || renderedKeys!=labelKeys)) {
                const auto pixels=frog::RenderObsKeyLabels(labelKeys);
                if(!pixels.empty()) {
                    if(!labelsTexture)labelsTexture=gs_texture_create(880,880,GS_RGBA,1,nullptr,GS_DYNAMIC);
                    if(labelsTexture){gs_texture_set_image(labelsTexture,pixels.data(),880*4,false);renderedKeys=labelKeys;}
                }
            }
            gs_texrender_reset(composed);
            if(gs_texrender_begin(composed,880,880)) {
                vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,880,0,880,-100,100);
                gs_blend_state_push();gs_enable_blending(true);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
                bool showingLaugh=false;
                if(laughFrame>=0 && laugh.decode(laughFrame)) {
                    if(!laughTexture)laughTexture=gs_texture_create(laugh.width,laugh.height,GS_RGBA,1,nullptr,GS_DYNAMIC);
                    if(laughTexture) {
                        gs_texture_set_image(laughTexture,laugh.rgba.data(),laugh.width*4,false);
                        auto *e=obs_get_base_effect(OBS_EFFECT_DEFAULT);
                        gs_effect_set_texture(gs_effect_get_param_by_name(e,"image"),laughTexture);
                        gs_matrix_push();gs_matrix_translate3f(frog::LaughDisplayX,0,0);
                        while(gs_effect_loop(e,"Draw"))gs_draw_sprite(laughTexture,0,frog::LaughDisplaySize,frog::LaughDisplaySize);
                        gs_matrix_pop();
                        showingLaugh=true;
                    }
                }
                if(!showingLaugh) {
                    if(separatedParts) {
                        parts[0].draw(held?1:0);
                        gs_blend_function_separate(GS_BLEND_DSTCOLOR,GS_BLEND_INVSRCALPHA,GS_BLEND_ZERO,GS_BLEND_ONE);
                        parts[3].draw(frog::LeftPose(held,unsigned(labelKeys.size())));
                        parts[3].draw(16+frog::RightPose(held,unsigned(labelKeys.size())));
                        gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
                        parts[1].draw(parts[1].count==3?frog::KeyboardIndex(unsigned(labelKeys.size())):unsigned(labelKeys.size())-4);
                    } else sprite(background.texture);
                    if(dynamicLabels && labelsTexture) {
                        auto *labelEffect=obs_get_base_effect(OBS_EFFECT_DEFAULT);
                        gs_effect_set_texture(gs_effect_get_param_by_name(labelEffect,"image"),labelsTexture);
                        while(gs_effect_loop(labelEffect,"Draw"))gs_draw_sprite(labelsTexture,0,880,880);
                    }

                    if(particlesEnabled){surface();particles(-1);}
                    if(separatedParts) {
                        parts[2].draw(frog::LeftPose(held,unsigned(labelKeys.size())));
                        parts[2].draw(16+frog::RightPose(held,unsigned(labelKeys.size())));
                    } else sprite(foreground.texture);
                    if(particlesEnabled)particles(1);
                }
                gs_blend_state_pop();gs_texrender_end(composed);dirty=false;
            }
        }
        auto *texture=gs_texrender_get_texture(composed);if(!texture)return;
        auto *e=obs_get_base_effect(OBS_EFFECT_DEFAULT);
        gs_effect_set_texture(gs_effect_get_param_by_name(e,"image"),texture);
        gs_blend_state_push();gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
        while(gs_effect_loop(e,"Draw"))gs_draw_sprite(texture,0,880,880);
        gs_blend_state_pop();
    }
};

// Dedicated native audio source: included in the same scene, with monitoring
// disabled. Viewers hear the approved WAV without playing it on the desktop.
struct LaughAudio {
    obs_source_t *source;
    std::mutex mutex;
    std::vector<int16_t> samples;
    std::thread worker;
    std::atomic<bool> stopping{false};
    explicit LaughAudio(obs_source_t *s):source(s) {}
    void update(obs_data_t *settings) {
        std::ifstream file(std::filesystem::u8path(obs_data_get_string(settings,"audio_file")),std::ios::binary);
        std::vector<int16_t> candidate;
        char riff[4]{},wave[4]{};uint32_t total=0;
        file.read(riff,4);file.read(reinterpret_cast<char*>(&total),4);file.read(wave,4);
        bool format=false;
        if(file && std::string(riff,4)=="RIFF" && std::string(wave,4)=="WAVE") {
            while(file) {
                char tag[4]{};uint32_t length=0;file.read(tag,4);file.read(reinterpret_cast<char*>(&length),4);
                if(!file || length>16*1024*1024)break;
                std::vector<char> chunk(length);file.read(chunk.data(),length);if(!file)break;
                if(std::string(tag,4)=="fmt " && length>=16) {
                    uint16_t code=0,channels=0,bits=0;uint32_t rate=0;
                    memcpy(&code,chunk.data(),2);memcpy(&channels,chunk.data()+2,2);
                    memcpy(&rate,chunk.data()+4,4);memcpy(&bits,chunk.data()+14,2);
                    format=code==1 && channels==1 && rate==44100 && bits==16;
                } else if(std::string(tag,4)=="data" && format && length%2==0) {
                    candidate.resize(length/2);memcpy(candidate.data(),chunk.data(),length);break;
                }
                if(length%2)file.ignore(1);
            }
        }
        std::lock_guard<std::mutex> lock(mutex);samples=std::move(candidate);
        if(samples.empty())blog(LOG_WARNING,"[milk-frog] Laughter audio unavailable.");
    }
    void start() {
        worker=std::thread([this] {
            std::array<int16_t,441> block{};
            uint64_t revision=~uint64_t(0);size_t cursor=0;
            auto deadline=std::chrono::steady_clock::now();
            while(!stopping) {
                const auto state=playbackSnapshot();
                if(state.revision!=revision) {
                    revision=state.revision;
                    cursor=state.playing?size_t((frog::NowNs()-state.started)*44100/1000000000ull):0;
                }
                block.fill(0);
                if(state.playing && obs_source_active(source)) {
                    std::lock_guard<std::mutex> lock(mutex);
                    for(size_t i=0;i<block.size() && cursor+i<samples.size();++i)block[i]=samples[cursor+i];
                }
                if(state.playing)cursor+=block.size();
                obs_source_audio audio{};audio.data[0]=reinterpret_cast<const uint8_t*>(block.data());
                audio.frames=DWORD(block.size());audio.speakers=SPEAKERS_MONO;
                audio.format=AUDIO_FORMAT_16BIT;audio.samples_per_sec=44100;audio.timestamp=os_gettime_ns();
                obs_source_output_audio(source,&audio);
                deadline+=std::chrono::milliseconds(10);
                if(deadline<std::chrono::steady_clock::now()-std::chrono::milliseconds(30))deadline=std::chrono::steady_clock::now();
                std::this_thread::sleep_until(deadline);
            }
        });
    }
    ~LaughAudio(){stopping=true;if(worker.joinable())worker.join();}
};
#ifdef MILK_FROG_EASY
struct NativeSource {
    Filter video;
    LaughAudio audio;
    explicit NativeSource(obs_source_t *source):video(source,true),audio(source) {}
    void update(obs_data_t *settings) {video.update(settings);audio.update(settings);}
};
void ImportResult(const std::filesystem::path& request,obs_data_t *data,const char *status,const char *message) {
    obs_data_set_string(data,"status",status);obs_data_set_string(data,"message",message);
    std::error_code error;std::filesystem::create_directories(easyFolder/L"results",error);
    const auto result=(easyFolder/L"results"/request.filename()).u8string();
    if(obs_data_save_json_safe(data,result.c_str(),"tmp",nullptr))std::filesystem::remove(request,error);
}
void ProcessImports(void *) {
    if(!frontendReady){importQueued=false;return;}
    // Module post-load precedes scene loading on startup. Wait for the frontend
    // to have a scene before judging queued destination names.
    auto *readyScene=currentScene?currentScene():nullptr;
    if(!readyScene){importQueued=false;return;}obs_source_release(readyScene);
    char *active=currentCollection?currentCollection():nullptr;
    if(!active){importQueued=false;return;}
    const std::string collection=active;bfree(active);
    std::error_code error;
    bool changed=false;
    for(const auto& entry:std::filesystem::directory_iterator(easyFolder/L"requests",error)) {
        if(entry.path().extension()!=L".json")continue;
        auto *request=obs_data_create_from_json_file(entry.path().u8string().c_str());
        if(!request)continue;
        const std::string wanted=obs_data_get_string(request,"collection");
        if(wanted!=collection){obs_data_release(request);continue;}
        const std::string sceneName=obs_data_get_string(request,"scene"),name=obs_data_get_string(request,"source");
        auto *sceneSource=obs_get_source_by_name(sceneName.c_str());
        auto *scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;
        auto *source=name.empty()?nullptr:obs_get_source_by_name(name.c_str());
        obs_sceneitem_t *existing=nullptr;
        if(scene && source)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t *item,void *data){
            auto **pair=static_cast<void**>(data);
            if(obs_sceneitem_get_source(item)==pair[0]){*static_cast<obs_sceneitem_t**>(pair[1])=item;return false;}return true;
        },std::array<void*,2>{source,&existing}.data());
        const bool update=std::string(obs_data_get_string(request,"action"))=="Update";
        if(!scene || name.empty() || (source && std::string(obs_source_get_id(source))!="milk_frog_source") || (update && !existing)) {
            ImportResult(entry.path(),request,"error","Target scene/source is missing or the source name belongs to another source type.");
        } else {
            if(!source)source=obs_source_create("milk_frog_source",name.c_str(),nullptr,nullptr);
            if(!source)ImportResult(entry.path(),request,"error","Unable to create the Milk Frog source.");
            else {
                if(update){auto *settings=obs_source_get_settings(source);obs_source_update(source,settings);obs_data_release(settings);}
                bool added=false;
                if(!existing) {
                    existing=obs_scene_add(scene,source);added=existing!=nullptr;
                    if(existing){
                        obs_video_info video{};obs_get_video_info(&video);
                        const float scale=std::min(400.f,float(video.base_width)*.3f)/880.f;
                        vec2 size{scale,scale},position{std::max(0.f,float(video.base_width)-880*scale-20),std::max(0.f,float(video.base_height)-712*scale-20)};
                        obs_sceneitem_set_scale(existing,&size);obs_sceneitem_set_pos(existing,&position);
                        obs_sceneitem_crop crop{0,0,0,168};obs_sceneitem_set_crop(existing,&crop);
                    }
                }
                obs_data_set_bool(request,"added",added);
                changed=changed || added || update;
                ImportResult(entry.path(),request,existing?"complete":"error",existing?"Milk Frog is ready in the specified scene.":"Unable to add the source to the scene.");
                blog(LOG_INFO,"[milk-frog-easy] %s / %s / %s: %s",collection.c_str(),sceneName.c_str(),name.c_str(),existing?"ready":"failed");
            }
        }
        if(source)obs_source_release(source);if(sceneSource)obs_source_release(sceneSource);obs_data_release(request);
    }
    if(changed){auto module=GetModuleHandleW(L"obs-frontend-api.dll");
        auto save=module?reinterpret_cast<void(*)()>(GetProcAddress(module,"obs_frontend_save")):nullptr;if(save)save();}
    importQueued=false;
}
#endif
}

MODULE_EXPORT bool obs_module_load(void) {
    hookThread=std::thread([]{
        MSG message; PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE); hookThreadId=GetCurrentThreadId();
        keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,keyboardProc,GetModuleHandleW(nullptr),0);
        if(!keyboardHook) blog(LOG_ERROR,"[milk-frog] keyboard hook unavailable: %lu",GetLastError());
        reloadLaughHotkey(true);
        heldKeys=frog::ObsHeldMask(gameplayKeys,playback.pressed);
        const UINT_PTR timer=SetTimer(nullptr,0,300,nullptr);
        while(GetMessageW(&message,nullptr,0,0)>0) {
            if(message.message==WM_TIMER && message.wParam==timer){
                reloadLaughHotkey();reloadGameplayBindings();
#ifdef MILK_FROG_EASY
                QueueImports();
#endif
            }
            else {TranslateMessage(&message);DispatchMessageW(&message);}
        }
        if(timer)KillTimer(nullptr,timer);
        if(keyboardHook)UnhookWindowsHookEx(keyboardHook);keyboardHook=nullptr;
    });
#ifdef MILK_FROG_EASY
    char *preset=obs_module_file("milk-frog.json");
    if(preset){easyFolder=std::filesystem::u8path(preset).parent_path();bfree(preset);}
    obs_source_info input{};
    input.id="milk_frog_source";input.type=OBS_SOURCE_TYPE_INPUT;input.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_AUDIO|OBS_SOURCE_CUSTOM_DRAW;
    input.get_name=[](void*){return "奶蛙（敲键、粒子、笑声一体）";};
    input.create=[](obs_data_t *s,obs_source_t *source)->void*{auto *n=new NativeSource(source);n->update(s);n->audio.start();return n;};
    input.destroy=[](void *p){delete static_cast<NativeSource*>(p);};
    input.update=[](void *p,obs_data_t *s){static_cast<NativeSource*>(p)->update(s);};
    input.video_tick=[](void *p,float dt){static_cast<NativeSource*>(p)->video.tick(dt);};
    input.video_render=[](void *p,gs_effect_t*){static_cast<NativeSource*>(p)->video.render();};
    input.get_width=[](void*)->uint32_t{return 880;};input.get_height=[](void*)->uint32_t{return 880;};
    input.get_defaults=[](obs_data_t *s){
        obs_data_set_default_bool(s,"particles",true);
        obs_data_set_default_string(s,"config_file",(easyFolder/L"milk-frog.json").u8string().c_str());
        obs_data_set_default_string(s,"audio_file",(easyFolder/L"laugh.wav").u8string().c_str());
    };
    input.get_properties=[](void*){auto *p=obs_properties_create();
        obs_properties_add_bool(p,"particles","黄色喷泉粒子");
        obs_properties_add_button(p,"key_setup","修改模式、键位与大笑快捷键…",[](obs_properties_t*,obs_property_t*,void*)->bool{
            ShellExecuteW(nullptr,L"open",(easyFolder/L"MilkFrogKeySetup.exe").c_str(),nullptr,easyFolder.c_str(),SW_SHOWNORMAL);return false;});return p;};
    obs_register_source(&input);
#else
    obs_source_info info{};
    info.id="milk_frog_native_animation";info.type=OBS_SOURCE_TYPE_FILTER;info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
    info.get_name=[](void*){return "奶蛙 DFJK · 原生动作与粒子";};
    info.create=[](obs_data_t *s,obs_source_t *source)->void* {auto *f=new Filter(source);f->update(s);return f;};
    info.destroy=[](void *p){delete static_cast<Filter*>(p);};
    info.update=[](void *p,obs_data_t *s){static_cast<Filter*>(p)->update(s);};
    info.video_tick=[](void *p,float dt){static_cast<Filter*>(p)->tick(dt);};
    info.video_render=[](void *p,gs_effect_t*){static_cast<Filter*>(p)->render();};
    info.get_defaults=[](obs_data_t *s){obs_data_set_default_bool(s,"particles",true);};
    info.get_properties=[](void*){
        auto *p=obs_properties_create();
        obs_properties_add_path(p,"config_file","奶蛙输入叠加配置",OBS_PATH_FILE,"JSON (*.json)",nullptr);
        obs_properties_add_bool(p,"particles","黄色喷泉粒子");return p;
    };
    obs_register_source(&info);
    obs_source_info audio{};
    audio.id="milk_frog_laugh_audio";audio.type=OBS_SOURCE_TYPE_INPUT;audio.output_flags=OBS_SOURCE_AUDIO;
    audio.get_name=[](void*){return "奶蛙 · F5 笑声音频";};
    audio.create=[](obs_data_t *s,obs_source_t *source)->void*{auto *a=new LaughAudio(source);a->update(s);a->start();return a;};
    audio.destroy=[](void *p){delete static_cast<LaughAudio*>(p);};
    audio.update=[](void *p,obs_data_t *s){static_cast<LaughAudio*>(p)->update(s);};
    audio.get_properties=[](void*){auto *p=obs_properties_create();obs_properties_add_path(p,"audio_file","笑声文件",OBS_PATH_FILE,"WAV (*.wav)",nullptr);return p;};
    obs_register_source(&audio);
#endif
    blog(LOG_INFO,"[milk-frog] Native input-overlay animation loaded (no browser/websocket).");
    return true;
}
MODULE_EXPORT void obs_module_unload(void) {
#ifdef MILK_FROG_EASY
    frontendReady=false;
#endif
    DWORD id=hookThreadId.load();if(id)PostThreadMessageW(id,WM_QUIT,0,0);
    if(hookThread.joinable())hookThread.join();
}
#ifdef MILK_FROG_EASY
MODULE_EXPORT void obs_module_post_load(void) {
    auto module=GetModuleHandleW(L"obs-frontend-api.dll");
    currentCollection=module?reinterpret_cast<CurrentCollection>(GetProcAddress(module,"obs_frontend_get_current_scene_collection")):nullptr;
    currentScene=module?reinterpret_cast<CurrentScene>(GetProcAddress(module,"obs_frontend_get_current_scene")):nullptr;
    frontendReady=currentCollection && currentScene && !easyFolder.empty();
}
#endif
