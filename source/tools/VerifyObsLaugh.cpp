// Exercise production render/audio code in libobs without installing hooks or sending keys.
#define MILK_FROG_EASY
#include "../obs-native/milk-frog-native.cpp"
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace {
std::atomic<unsigned> audibleBlocks{0};
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
std::vector<uint8_t> capture(Filter& video) {
    GraphicsLock lock;
    auto *render=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
    require(gs_texrender_begin(render,880,880),"render target");
    vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,880,0,880,-100,100);
    video.render();gs_texrender_end(render);
    auto *stage=gs_stagesurface_create(880,880,GS_RGBA);
    gs_stage_texture(stage,gs_texrender_get_texture(render));
    uint8_t *pixels=nullptr;uint32_t stride=0;
    require(gs_stagesurface_map(stage,&pixels,&stride),"GPU readback");
    std::vector<uint8_t> result(880*880*4);
    for(unsigned y=0;y<880;++y)memcpy(result.data()+y*880*4,pixels+y*stride,880*4);
    gs_stagesurface_unmap(stage);gs_stagesurface_destroy(stage);gs_texrender_destroy(render);
    return result;
}
void playAt(unsigned milliseconds) {
    std::lock_guard<std::mutex> lock(playbackMutex);
    playback.setHotkey(frog::KeyChord());
    playback.key(VK_F5,true,false,false,frog::NowNs()-uint64_t(milliseconds)*1000000);
}
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=5)return 2;
    const std::filesystem::path data=argv[1],original=argv[2],obsRoot=argv[3],out=argv[4];
    try {
        require(obs_startup("en-US",nullptr,nullptr),"OBS startup");
        obs_add_data_path((obsRoot/L"data/libobs").u8string().c_str());
        const auto graphics=(obsRoot/L"bin/64bit/libobs-d3d11.dll").u8string();
        obs_video_info info{};info.graphics_module=graphics.c_str();info.fps_num=60;info.fps_den=1;
        info.base_width=info.output_width=880;info.base_height=info.output_height=880;
        info.output_format=VIDEO_FORMAT_RGBA;info.colorspace=VIDEO_CS_709;info.range=VIDEO_RANGE_FULL;
        require(obs_reset_video(&info)==0,"OBS video");
        obs_audio_info audioInfo{44100,SPEAKERS_MONO};require(obs_reset_audio(&audioInfo),"OBS audio");
        obs_source_info audioSource{};audioSource.id="frog_audio_verification";
        audioSource.type=OBS_SOURCE_TYPE_INPUT;audioSource.output_flags=OBS_SOURCE_AUDIO;
        audioSource.get_name=[](void*){return "Audio verification";};
        audioSource.create=[](obs_data_t*,obs_source_t *source)->void*{return source;};
        audioSource.destroy=[](void*){};obs_register_source(&audioSource);
        auto *source=obs_source_create(audioSource.id,"Audio verification",nullptr,nullptr);
        require(source!=nullptr,"audio source");obs_source_inc_active(source);
        obs_source_add_audio_capture_callback(source,[](void*,obs_source_t*,const audio_data *block,bool){
            const auto *samples=reinterpret_cast<const float*>(block->data[0]);
            for(unsigned i=0;i<block->frames;++i)if(std::abs(samples[i])>.00001f){++audibleBlocks;break;}
        },nullptr);
        double squaredError=0;uint64_t channels=0;
        {
            Filter video(nullptr,true);LaughAudio audio(source);
            auto *settings=obs_data_create();
            obs_data_set_string(settings,"config_file",(data/L"milk-frog.json").u8string().c_str());
            obs_data_set_string(settings,"audio_file",(data/L"laugh.wav").u8string().c_str());
            video.update(settings);audio.update(settings);obs_data_release(settings);
            require(video.loaded && video.laughLoaded && !audio.samples.empty(),"production assets");
            frog::LaughFrames allFrames;require(allFrames.load(data/L"laugh-frames.mfa"),"compact pack");
            for(unsigned i=0;i<allFrames.count();++i)require(allFrames.decode(i),"WIC frame decode");
            for(int frame:{0,108,215}) {
                require(video.laugh.load(data/L"laugh-frames.mfa"),"compact reload");
                video.laughFrame=frame;video.dirty=true;const auto compact=capture(video);
                require(video.laugh.load(original),"original reload");
                video.laughFrame=frame;video.dirty=true;const auto reference=capture(video);
                for(size_t p=0;p<compact.size();p+=4) {
                    require(compact[p+3]==reference[p+3],"GPU alpha differs");
                    if(compact[p+3])for(unsigned c=0;c<3;++c){const double d=int(compact[p+c])-int(reference[p+c]);squaredError+=d*d;++channels;}
                }
                if(frame==108){std::ofstream image(out/L"laugh.rgba",std::ios::binary);image.write(reinterpret_cast<const char*>(compact.data()),compact.size());}
            }
            require(video.laugh.load(data/L"laugh-frames.mfa"),"compact playback reload");
            audio.start();playAt(3000);video.tick(1.f/60);
            require(video.laughFrame>=72 && video.laughFrame<=74,"shared-clock frame selection");
            capture(video);Sleep(350);require(audibleBlocks>0,"laughter audio is silent");
            {std::lock_guard<std::mutex> lock(playbackMutex);playback.key('D',true,true,false,frog::NowNs());}
            video.tick(1.f/60);require(video.laughFrame==-1,"gameplay did not cancel video");
            Sleep(100);const auto silent=audibleBlocks.load();Sleep(150);
            require(audibleBlocks==silent,"gameplay did not cancel audio");
            playAt(9100);video.tick(1.f/60);require(video.laughFrame==-1,"video exceeded nine seconds");
            require(!playbackSnapshot().playing,"playback did not finish");
        }
        obs_source_dec_active(source);obs_source_release(source);obs_wait_for_destroy_queue();obs_shutdown();
        std::ofstream report(out/L"laugh-report.json");
        report<<"{\"passed\":true,\"decodedFrames\":216,\"gpuAlphaError\":0,\"visiblePremultipliedRGBRMSE\":"<<std::sqrt(squaredError/channels)
              <<",\"checks\":[\"production GPU render at frames 0/108/215\",\"shared playback clock\",\"audible WAV output\",\"gameplay cancels video and audio\",\"nine-second completion\"]}";
        std::cout<<"PASS: compact frames, production GPU rendering, shared-clock audio and cancellation.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
