#pragma once
#include "LaughHotkey.hpp"
#include <sstream>
#include "LaneLayout.hpp"

namespace frog {
using ObsBindings=std::vector<unsigned>;
inline const ObsBindings DefaultObsBindings{'D','F','J','K'};
inline ObsBindings DefaultObsKeys(unsigned count){auto keys=DefaultLaneKeys(count);return ObsBindings(keys.begin(),keys.end());}
inline bool BindingMatches(unsigned binding,unsigned code) {
    return binding==code || ((binding==VK_CONTROL || binding==VK_SHIFT || binding==VK_MENU) && binding==ChordKey(code));
}
inline bool ValidObsBindings(const ObsBindings& keys) {
    if(!ValidLaneCount(unsigned(keys.size())))return false;
    for(size_t i=0;i<keys.size();++i) {
        if(!keys[i] || keys[i]>=256)return false;
        for(size_t j=0;j<i;++j)if(BindingMatches(keys[i],keys[j]) || BindingMatches(keys[j],keys[i]))return false;
    }
    return true;
}
inline bool ReadObsBindings(const std::filesystem::path& path,ObsBindings& result) {
    std::ifstream file(path,std::ios::binary);
    if(!file)return false;
    std::string text((std::istreambuf_iterator<char>(file)),{});
    if(text.size()>4096)return false;
    auto key=text.find("\"keys\"");if(key==std::string::npos)return false;
    auto colon=text.find(':',key+6);if(colon==std::string::npos)return false;
    std::istringstream values(text.substr(colon+1));char delimiter=0;
    if(!(values>>delimiter) || delimiter!='[')return false;
    ObsBindings candidate;
    for(unsigned i=0;i<MaxLanes;++i) {
        unsigned code=0;if(!(values>>code>>delimiter))return false;
        candidate.push_back(code);
        if(delimiter==']')break;if(delimiter!=',')return false;
    }
    if(delimiter!=']' || !ValidObsBindings(candidate))return false;
    result=candidate;return true;
}
inline unsigned ObsHeldMask(const ObsBindings& keys,const std::array<bool,256>& down) {
    unsigned mask=0;
    for(unsigned i=0;i<keys.size();++i)if(keys[i]==VK_LWIN?down[VK_LWIN]:KeyChord::pressed(down,keys[i]))mask|=1u<<i;
    return mask;
}
}
