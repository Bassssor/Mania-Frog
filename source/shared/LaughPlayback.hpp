#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "LaughHotkey.hpp"

namespace frog {
constexpr unsigned LaughDisplaySize=712;
constexpr float LaughDisplayX=84.f;
inline uint64_t NowNs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
// A physical press starts once; keyboard repeat does not restart playback.
// Gameplay keys take precedence, including when F5 itself is rebound.
struct LaughPlayback {
    bool playing=false, triggerDown=false;
    KeyChord hotkey;
    std::array<bool,256> pressed{};
    uint64_t started=0, revision=0;
    void stop() { if(playing){playing=false;++revision;} }
    void syncPressed() {
        for(unsigned key=0;key<pressed.size();++key)
            pressed[key]=key!=VK_CONTROL && key!=VK_SHIFT && key!=VK_MENU && (GetAsyncKeyState(key)&0x8000)!=0;
        triggerDown=hotkey.matches(pressed);
    }
    void setHotkey(const KeyChord& chord) {hotkey=chord;pressed.fill(false);triggerDown=false;}
    void key(unsigned code,bool down,bool bound,bool gameplayHeld,uint64_t now) {
        if(bound && down)stop();
        if(code>=pressed.size())return;
        pressed[code]=down;
        const bool complete=hotkey.matches(pressed);
        const bool fresh=down && !triggerDown && complete;
        triggerDown=complete;
        if(fresh && !bound && !gameplayHeld){playing=true;started=now;++revision;}
    }
    int frame(uint64_t now,unsigned frames=216,unsigned fps=24) {
        if(!playing)return -1;
        const uint64_t elapsed=now>=started?now-started:0;
        const uint64_t index=elapsed/(1000000000ull/fps);
        if(index>=frames){stop();return -1;}
        return int(index);
    }
};

// Indexed lossless PNG frames. Decode one frame at a time with Windows WIC;
// neither a browser nor an external player is required at runtime.
class LaughFrames {
    struct Entry {uint64_t offset;uint32_t bytes;};
    std::ifstream file;
    std::vector<Entry> entries;
public:
    unsigned width=0,height=0,fps=0;
    std::vector<unsigned char> rgba;
    int current=-1;
    bool load(const std::filesystem::path& path) {
        file.close();file.clear();entries.clear();current=-1;
        file.open(path,std::ios::binary);if(!file)return false;
        std::array<char,8> magic{};std::array<uint32_t,4> header{};
        file.read(magic.data(),8);file.read(reinterpret_cast<char*>(header.data()),16);
        if(magic!=std::array<char,8>{'M','F','L','A','U','G','H','1'} ||
           header[0]!=720 || header[1]!=720 || header[2]!=24 || header[3]!=216)return false;
        width=header[0];height=header[1];fps=header[2];entries.resize(header[3]);
        file.seekg(0,std::ios::end);const auto length=uint64_t(file.tellg());file.seekg(24);
        for(auto& e:entries) {
            file.read(reinterpret_cast<char*>(&e.offset),8);file.read(reinterpret_cast<char*>(&e.bytes),4);
            if(e.offset<24+entries.size()*12 || e.bytes==0 || e.bytes>8*1024*1024 || e.offset>length || e.bytes>length-e.offset)return false;
        }
        if(!file)return false;
        rgba.resize(size_t(width)*height*4);return true;
    }
    unsigned count()const{return unsigned(entries.size());}
    bool decode(int frame) {
        if(frame==current)return true;
        if(frame<0 || unsigned(frame)>=entries.size())return false;
        const auto& e=entries[frame];std::vector<unsigned char> bytes(e.bytes);
        file.clear();file.seekg(e.offset);file.read(reinterpret_cast<char*>(bytes.data()),e.bytes);if(!file)return false;
        // OBS updates/destroys filters on different threads. Balance COM on
        // the decoding thread itself rather than in the object's destructor.
        struct ComScope {HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            ~ComScope(){if(SUCCEEDED(hr))CoUninitialize();}} com;
        if(FAILED(com.hr) && com.hr!=RPC_E_CHANGED_MODE)return false;
        IWICImagingFactory *factory=nullptr;
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
        IWICStream *stream=nullptr;IWICBitmapDecoder *decoder=nullptr;
        IWICBitmapFrameDecode *bitmap=nullptr;IWICFormatConverter *converter=nullptr;
        auto hr=factory->CreateStream(&stream);
        if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(bytes.data(),DWORD(bytes.size()));
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
        if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&bitmap);
        UINT w=0,h=0;if(SUCCEEDED(hr))hr=bitmap->GetSize(&w,&h);
        if(SUCCEEDED(hr) && (w!=width || h!=height))hr=E_FAIL;
        if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
        if(SUCCEEDED(hr))hr=converter->Initialize(bitmap,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        if(SUCCEEDED(hr))hr=converter->CopyPixels(nullptr,width*4,UINT(rgba.size()),rgba.data());
        if(converter)converter->Release();if(bitmap)bitmap->Release();if(decoder)decoder->Release();if(stream)stream->Release();
        factory->Release();
        if(FAILED(hr))return false;
        // Both SFML's layered window and OBS blend premultiplied alpha.
        for(size_t i=0;i<rgba.size();i+=4)for(int c=0;c<3;++c)
            rgba[i+c]=uint8_t((unsigned(rgba[i+c])*rgba[i+3]+127)/255);
        current=frame;return true;
    }
};
}
