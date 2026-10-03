#pragma once
#include <windows.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace frog {
inline unsigned ChordKey(unsigned code) {
    if(code==VK_LCONTROL || code==VK_RCONTROL)return VK_CONTROL;
    if(code==VK_LSHIFT || code==VK_RSHIFT)return VK_SHIFT;
    if(code==VK_LMENU || code==VK_RMENU)return VK_MENU;
    if(code==VK_RWIN)return VK_LWIN;
    return code;
}
struct KeyChord {
    std::array<unsigned,8> codes{VK_F5};
    unsigned count=1;
    bool operator==(const KeyChord& other)const {
        return count==other.count && std::equal(codes.begin(),codes.begin()+count,other.codes.begin());
    }
    bool add(unsigned code) {
        code=ChordKey(code);
        if(code==0 || code>=256)return false;
        if(std::find(codes.begin(),codes.begin()+count,code)!=codes.begin()+count)return true;
        if(count==codes.size())return false;
        codes[count++]=code;std::sort(codes.begin(),codes.begin()+count);return true;
    }
    std::string text()const {
        std::string result;
        for(unsigned i=0;i<count;++i){if(i)result+='+';result+=std::to_string(codes[i]);}
        return result;
    }
    static bool parse(const std::string& text,KeyChord& out) {
        KeyChord candidate;candidate.count=0;
        size_t offset=0;
        while(offset<text.size()) {
            const auto end=text.find('+',offset);const auto part=text.substr(offset,end==std::string::npos?end:end-offset);
            try {
                size_t used=0;const auto code=std::stoul(part,&used);
                if(used!=part.size() || code>=256 || !candidate.add(unsigned(code)))return false;
            }catch(...){return false;}
            if(end==std::string::npos)break;
            offset=end+1;if(offset==text.size())return false;
        }
        if(!candidate.count)return false;
        out=candidate;return true;
    }
    static bool pressed(const std::array<bool,256>& state,unsigned code) {
        if(code==VK_CONTROL)return state[VK_CONTROL] || state[VK_LCONTROL] || state[VK_RCONTROL];
        if(code==VK_SHIFT)return state[VK_SHIFT] || state[VK_LSHIFT] || state[VK_RSHIFT];
        if(code==VK_MENU)return state[VK_MENU] || state[VK_LMENU] || state[VK_RMENU];
        if(code==VK_LWIN)return state[VK_LWIN] || state[VK_RWIN];
        return state[code];
    }
    bool matches(const std::array<bool,256>& state)const {
        if(!count)return false;
        for(unsigned i=0;i<count;++i)if(!pressed(state,codes[i]))return false;
        for(unsigned modifier:{unsigned(VK_CONTROL),unsigned(VK_SHIFT),unsigned(VK_MENU),unsigned(VK_LWIN)}) {
            const bool required=std::find(codes.begin(),codes.begin()+count,modifier)!=codes.begin()+count;
            if(pressed(state,modifier)!=required)return false;
        }
        return true;
    }
};
inline KeyChord ReadLaughHotkey(const std::filesystem::path& path) {
    KeyChord result;std::ifstream file(path);std::string line;
    while(std::getline(file,line))if(line.rfind("laugh_keys=",0)==0) {
        auto value=line.substr(11);if(!value.empty() && value.back()=='\r')value.pop_back();
        KeyChord::parse(value,result);
    }
    return result;
}
inline std::filesystem::path LaughConfigPath() {
    std::array<wchar_t,32768> local{};
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local.data(),DWORD(local.size()));
    if(length && length<local.size())return std::filesystem::path(local.data())/L"MilkFrog"/L"config.txt";
    return L"config.txt";
}
struct ChordCapture {
    KeyChord chord;
    std::array<bool,256> down{};
    bool overflow=false;
    void reset(){chord.count=0;down.fill(false);overflow=false;}
    bool key(unsigned code,bool held) {
        if(code>=down.size())return false;
        down[code]=held;
        if(held && !chord.add(code))overflow=true;
        return !held && chord.count && std::none_of(down.begin(),down.end(),[](bool value){return value;});
    }
};
}
