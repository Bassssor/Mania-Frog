// Headless integration test against the installed libobs; no OBS UI or profile.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <obs.h>
#include <util/bmem.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <stdexcept>
#ifdef FROG_FRONTEND_STUB
std::string collection="Verification";
extern "C" __declspec(dllexport) char *obs_frontend_get_current_scene_collection(){return bstrdup(collection.c_str());}
extern "C" __declspec(dllexport) obs_source_t *obs_frontend_get_current_scene(){return obs_get_source_by_name("Target");}
extern "C" __declspec(dllexport) void obs_frontend_save(){}
extern "C" __declspec(dllexport) void frog_set_collection(const char *name){collection=name;}
#else
namespace {
std::mutex mutex;std::queue<std::pair<obs_task_t,void*>> tasks;
std::atomic<int> audioBlocks{0};
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void pump(int milliseconds) {
    auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    do {
        std::pair<obs_task_t,void*> task{};
        {std::lock_guard<std::mutex> lock(mutex);if(!tasks.empty()){task=tasks.front();tasks.pop();}}
        if(task.first)task.first(task.second);else Sleep(5);
    }while(std::chrono::steady_clock::now()<until);
}
int count(obs_scene_t *scene){int result=0;obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*,void *p){++*static_cast<int*>(p);return true;},&result);return result;}
void request(const std::filesystem::path& data,const char *id,const char *collection,const char *source,const char *action="Add") {
    auto *r=obs_data_create();obs_data_set_string(r,"requestId",id);obs_data_set_string(r,"collection",collection);
    obs_data_set_string(r,"scene","Target");obs_data_set_string(r,"source",source);obs_data_set_string(r,"action",action);
    check(obs_data_save_json(r,(data/L"requests"/(std::string(id)+".json")).u8string().c_str()),"request write");obs_data_release(r);
}
void result(const std::filesystem::path& data,const char *id,const char *status) {
    pump(800);auto *r=obs_data_create_from_json_file((data/L"results"/(std::string(id)+".json")).u8string().c_str());
    check(r!=nullptr,"import did not return a result");const std::string actual=obs_data_get_string(r,"status");obs_data_release(r);check(actual==status,"unexpected import result");
}
void snapshot(obs_source_t *source,const std::filesystem::path& path){
    obs_enter_graphics();
    auto *render=gs_texrender_create(GS_RGBA,GS_ZS_NONE);check(gs_texrender_begin(render,880,880),"snapshot texture");
    vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,880,0,880,-100,100);
    obs_source_video_render(source);gs_texrender_end(render);
    auto *stage=gs_stagesurface_create(880,880,GS_RGBA);gs_stage_texture(stage,gs_texrender_get_texture(render));
    uint8_t *pixels=nullptr;uint32_t stride=0;check(gs_stagesurface_map(stage,&pixels,&stride),"snapshot map");
    std::ofstream out(path,std::ios::binary);for(int y=0;y<880;++y)out.write(reinterpret_cast<char*>(pixels+y*stride),880*4);
    gs_stagesurface_unmap(stage);gs_stagesurface_destroy(stage);gs_texrender_destroy(render);obs_leave_graphics();
}
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=5)return 2;
    const std::filesystem::path pluginsRoot=argv[1],data=argv[2],obsRoot=argv[3],output=argv[4];
    try{
        std::filesystem::create_directories(data/L"requests");std::filesystem::create_directories(output);
        check(obs_startup("en-US",nullptr,nullptr),"obs_startup");
        obs_add_data_path((obsRoot/L"data"/L"libobs").u8string().c_str());
        obs_set_ui_task_handler([](obs_task_t task,void *param,bool){std::lock_guard<std::mutex> lock(mutex);tasks.push({task,param});});
        const auto graphics=(obsRoot/L"bin"/L"64bit"/L"libobs-d3d11.dll").u8string();
        obs_video_info video{};video.graphics_module=graphics.c_str();video.fps_num=60;video.fps_den=1;
        video.base_width=1280;video.base_height=720;video.output_width=1280;video.output_height=720;
        video.output_format=VIDEO_FORMAT_RGBA;video.colorspace=VIDEO_CS_709;video.range=VIDEO_RANGE_FULL;video.scale_type=OBS_SCALE_BILINEAR;
        check(obs_reset_video(&video)==0,"obs_reset_video");obs_audio_info audio{44100,SPEAKERS_MONO};check(obs_reset_audio(&audio),"obs_reset_audio");
        const auto stub=LoadLibraryW((output/L"obs-frontend-api.dll").c_str());check(stub!=nullptr,"frontend stub");
        // OBS 33's frontend passes this directory layout to obs_load_plugins.
        // Resolve the function at runtime because the build headers predate this API.
        struct PluginPath {const char *binary;const char *data;int kind;};
        struct PluginFailures {char **names;size_t count;};
        auto loadPlugins=reinterpret_cast<void(*)(const PluginPath*,PluginFailures*)>(
            GetProcAddress(GetModuleHandleW(L"obs.dll"),"obs_load_plugins"));
        check(loadPlugins!=nullptr,"OBS 33 plugin scanner API unavailable");
        const auto pattern=(pluginsRoot/L"%module%").generic_u8string();
        const auto dataPattern=pattern+"/data";
        const PluginPath pluginPath{pattern.c_str(),dataPattern.c_str(),2};
        PluginFailures failures{};
        loadPlugins(&pluginPath,&failures);
        check(failures.count==0,"OBS plugin scan reported a load failure");
        bfree(failures.names);
        check(obs_get_module("milk-frog-easy")!=nullptr,"OBS scanner did not find the installed plugin");
        obs_post_load_modules();
        request(data,"startup","Verification","Frog");pump(800);
        check(std::filesystem::is_regular_file(data/L"requests"/L"startup.json"),"startup request was consumed before scene loading");
        auto *target=obs_scene_create("Target"),*other=obs_scene_create("Untouched");
        obs_set_output_source(0,obs_scene_get_source(target));
        result(data,"startup","complete");
        obs_sceneitem_remove(obs_scene_find_source(target,"Frog"));
        request(data,"wrong-collection","Another collection","Deferred");pump(800);check(count(target)==0 && count(other)==0,"wrong collection changed a scene");
        request(data,"first","Verification","Frog");result(data,"first","complete");check(count(target)==1 && count(other)==0,"targeted scene insertion");
        auto *source=obs_get_source_by_name("Frog");check(source && std::string(obs_source_get_id(source))=="milk_frog_source","native source type");
        check((obs_source_get_output_flags(source)&(OBS_SOURCE_AUDIO|OBS_SOURCE_VIDEO))==(OBS_SOURCE_AUDIO|OBS_SOURCE_VIDEO),"unified audio/video flags");
        check(obs_source_get_monitoring_type(source)==OBS_MONITORING_TYPE_NONE,"audio monitoring must default off");
        obs_source_add_audio_capture_callback(source,[](void*,obs_source_t*,const audio_data*,bool){++audioBlocks;},nullptr);
        pump(300);snapshot(source,output/L"idle.rgba");check(audioBlocks>0,"integrated audio is not producing blocks");
        auto *item=obs_scene_find_source(target,"Frog");vec2 position{123,234};obs_sceneitem_set_pos(item,&position);obs_sceneitem_set_visible(item,false);
        request(data,"again","Verification","Frog");result(data,"again","complete");check(count(target)==1,"duplicate import");
        vec2 after{};obs_sceneitem_get_pos(item,&after);check(after.x==123 && after.y==234 && !obs_sceneitem_visible(item),"reimport changed transform or visibility");
        request(data,"update","Verification","Frog","Update");result(data,"update","complete");check(count(target)==1,"update added a source");
        obs_sceneitem_get_pos(item,&after);check(after.x==123 && after.y==234 && !obs_sceneitem_visible(item),"update changed transform or visibility");
        request(data,"missing-update","Verification","Missing","Update");result(data,"missing-update","error");
        request(data,"collision","Verification","Untouched");result(data,"collision","error");check(count(other)==0,"foreign source changed");
        auto setCollection=reinterpret_cast<void(*)(const char*)>(GetProcAddress(stub,"frog_set_collection"));setCollection("Another collection");
        result(data,"wrong-collection","complete");check(count(target)==2,"deferred collection import");
        std::ofstream report(output/L"native-report.json");report<<"{\"passed\":true,\"obsVersion\":\""<<obs_get_version_string()<<"\",\"checks\":[\"OBS 33 plugin discovery and load\",\"wait for frontend scene loading\",\"explicit scene targeting\",\"collection isolation\",\"native render\",\"integrated audio\",\"monitoring off\",\"idempotent add\",\"update preserves layout\",\"foreign source collision rejected\",\"missing update rejected\",\"deferred import\"]}";report.close();
        obs_set_output_source(0,nullptr);
        obs_source_remove(obs_scene_get_source(target));obs_source_remove(obs_scene_get_source(other));
        obs_source_release(source);obs_scene_release(target);obs_scene_release(other);
        pump(300);obs_wait_for_destroy_queue();obs_shutdown();
        std::cout<<"PASS: native render, audio and explicit-target imports using the installed libobs.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
#endif
