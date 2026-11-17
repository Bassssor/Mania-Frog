// Render the production OBS filter in an isolated libobs graphics context.
// No installed plugin, user scene, keyboard event, or OBS setting is changed.
#include "../obs-native/milk-frog-native.cpp"
#include <iostream>
#include <stdexcept>

namespace {
Filter* activeFilter=nullptr;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<unsigned char> readPng(const std::filesystem::path& path) {
    Gdiplus::Bitmap image(path.c_str());require(image.GetLastStatus()==Gdiplus::Ok,"Reference PNG unavailable");
    require(image.GetWidth()==880 && image.GetHeight()==880,"Reference size mismatch");
    Gdiplus::Rect box(0,0,880,880);Gdiplus::BitmapData data{};
    require(image.LockBits(&box,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&data)==Gdiplus::Ok,"Reference pixels unavailable");
    std::vector<unsigned char> result(880*880*4);
    for(unsigned y=0;y<880;++y)for(unsigned x=0;x<880;++x) {
        const auto* src=static_cast<unsigned char*>(data.Scan0)+y*data.Stride+x*4;auto* dst=result.data()+(y*880+x)*4;
        dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=src[3];
    }
    image.UnlockBits(&data);return result;
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;
    const std::filesystem::path root=std::filesystem::absolute(argv[1]),obs=std::filesystem::absolute(argv[2]);
    const auto out=root/L"build/obs-parts-verification";std::filesystem::create_directories(out);
    try {
        frog::GdiSession gdi;
        require(obs_startup("en-US",(out/L"config").u8string().c_str(),nullptr),"libobs startup failed");
        obs_add_data_path(((obs/L"data/libobs").u8string()+"/").c_str());
        const auto graphics=(obs/L"bin/64bit/libobs-d3d11.dll").u8string();
        obs_video_info video{};video.graphics_module=graphics.c_str();video.fps_num=60;video.fps_den=1;
        video.base_width=video.base_height=video.output_width=video.output_height=880;
        video.output_format=VIDEO_FORMAT_RGBA;video.colorspace=VIDEO_CS_709;video.range=VIDEO_RANGE_FULL;video.scale_type=OBS_SCALE_BILINEAR;
        require(obs_reset_video(&video)==OBS_VIDEO_SUCCESS,"Isolated graphics initialization failed");
        obs_source_info parent{};parent.id="input-overlay";parent.type=OBS_SOURCE_TYPE_INPUT;parent.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
        parent.get_name=[](void*){return "Verification input";};parent.create=[](obs_data_t*,obs_source_t*)->void*{return reinterpret_cast<void*>(1);};
        parent.destroy=[](void*){};parent.get_width=[](void*){return 880u;};parent.get_height=[](void*){return 880u;};parent.video_render=[](void*,gs_effect_t*){};
        obs_register_source(&parent);
        obs_source_info filter{};filter.id="verification-milk-frog";filter.type=OBS_SOURCE_TYPE_FILTER;filter.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
        filter.get_name=[](void*){return "Verification filter";};
        filter.create=[](obs_data_t* data,obs_source_t* source)->void*{activeFilter=new Filter(source);activeFilter->update(data);return activeFilter;};
        filter.destroy=[](void* instance){delete static_cast<Filter*>(instance);};
        filter.video_render=[](void* instance,gs_effect_t*){static_cast<Filter*>(instance)->render();};
        obs_register_source(&filter);
        auto* settings=obs_data_create();obs_data_set_bool(settings,"particles",false);
        obs_data_set_string(settings,"config_file",(root.parent_path()/L"obs-input-overlay/milk-frog.json").u8string().c_str());
        auto* source=obs_source_create_private("input-overlay","Verification input",nullptr);
        auto* native=obs_source_create_private("verification-milk-frog","Verification filter",settings);
        obs_source_filter_add(source,native);obs_data_release(settings);
        require(activeFilter && activeFilter->loaded,"Production filter could not load exported parts");
        unsigned combinations=0,maxAlpha=0,maxRgb=0;
        {
            GraphicsLock graphicsLock;
            auto* target=gs_texrender_create(GS_RGBA,GS_ZS_NONE);auto* stage=gs_stagesurface_create(880,880,GS_RGBA);
            for(unsigned lanes=4;lanes<=8;++lanes)for(unsigned mask=0;mask<(1u<<lanes);++mask) {
                auto& f=*activeFilter;frog::LaneCount=lanes;f.labelKeys=frog::DefaultObsKeys(lanes);f.dynamicLabels=false;f.held=mask;f.dirty=true;
                gs_texrender_reset(target);require(gs_texrender_begin(target,880,880),"Capture target unavailable");
                vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,880,0,880,-100,100);f.render();gs_texrender_end(target);
                gs_stage_texture(stage,gs_texrender_get_texture(target));uint8_t* mapped=nullptr;uint32_t stride=0;
                require(gs_stagesurface_map(stage,&mapped,&stride),"GPU capture failed");
                std::vector<unsigned char> actual(880*880*4);
                for(unsigned y=0;y<880;++y)std::copy(mapped+y*stride,mapped+y*stride+880*4,actual.data()+y*880*4);
                gs_stagesurface_unmap(stage);
                const auto frames=root/L"build/obs-key-setup/frames"/std::to_wstring(lanes);
                auto expected=readPng(frames/(L"background-"+std::to_wstring(mask)+L".png"));
                const auto front=readPng(frames/(L"foreground-"+std::to_wstring(mask)+L".png"));
                for(size_t i=0;i<expected.size();i+=4)for(unsigned c=0;c<4;++c)
                    expected[i+c]=static_cast<unsigned char>(std::min(255u,unsigned(front[i+c])+(unsigned(expected[i+c])*(255-front[i+3])+127)/255));
                unsigned alpha=0,rgb=0;
                for(size_t i=0;i<actual.size();i+=4) {
                    alpha=std::max(alpha,unsigned(std::abs(int(actual[i+3])-expected[i+3])));
                    if(expected[i+3]==255)for(unsigned c=0;c<3;++c)rgb=std::max(rgb,unsigned(std::abs(int(actual[i+c])-expected[i+c])));
                }
                if(alpha>2 || rgb>4){std::cerr<<"Mismatch "<<lanes<<"K mask="<<mask<<" alpha="<<alpha<<" RGB="<<rgb<<'\n';throw std::runtime_error("OBS parts differ from native desktop layers");}
                maxAlpha=std::max(maxAlpha,alpha);maxRgb=std::max(maxRgb,rgb);++combinations;
                if(mask==0 || mask==((1u<<lanes)-1))frog::SaveObsLabels(out/(std::to_wstring(lanes)+L"k-"+std::to_wstring(mask)+L".png"),actual);
            }
            gs_stagesurface_destroy(stage);gs_texrender_destroy(target);
        }
        obs_source_filter_remove(source,native);obs_source_release(native);obs_source_release(source);obs_shutdown();
        std::ofstream(out/L"report.json")<<"{\"passed\":true,\"combinations\":"<<combinations<<",\"maxAlphaError\":"<<maxAlpha<<",\"maxOpaqueRgbError\":"<<maxRgb<<",\"isolatedLibobsGpu\":true}\n";
        std::cout<<"PASS: "<<combinations<<" production OBS GPU compositions; alpha error "<<maxAlpha<<", RGB error "<<maxRgb<<".\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
