#pragma once
#include <array>
#include <cmath>
#include <vector>
#include "ScenePlane.hpp"

namespace frog {
inline unsigned LaneCount=4;
constexpr unsigned MaxLanes=8;
inline bool ValidLaneCount(unsigned count){return count>=4 && count<=MaxLanes;}
inline unsigned KeyboardIndex(unsigned count=LaneCount){return count==4?0:count<=6?1:2;}
inline unsigned KeyboardLaneCount(unsigned count=LaneCount){return 4+2*KeyboardIndex(count);}
inline std::vector<int> DefaultLaneKeys(unsigned count) {
    if(count==5)return {'S','D','F','J','K'};
    if(count==6)return {'S','D','F','J','K','L'};
    if(count==7)return {'A','S','D','F','J','K','L'};
    if(count==8)return {'A','S','D','F','J','K','L',0xBA};
    return {'D','F','J','K'};
}
struct LanePoint {float x,y;};
inline LanePoint LaneCenter(unsigned slot,unsigned count=LaneCount) {
    constexpr LanePoint four[]={{646,572},{611,564},{488,545},{454,541}};
    constexpr LanePoint six[]={{681,577},{646,572},{611,564},{488,545},{454,541},{422,536}};
    constexpr LanePoint eight[]={{716,584},{681,577},{646,572},{611,564},{488,545},{454,541},{422,536},{387,528}};
    return count==4?four[slot]:(count<=6?six[slot]:eight[slot]);
}
inline LanePoint LaneKeyOrigin(unsigned slot,unsigned count=LaneCount) {
    const auto p=LaneCenter(slot,count);
    return {p.x,558.f+plane::UY*(p.x-560.f)};
}
inline LanePoint LaneLabelCenter(unsigned slot,unsigned count=LaneCount) {
    const auto p=LaneKeyOrigin(slot,count);
    const auto front=plane::Project(0.f,22.f);
    return {p.x+front.x,p.y+front.y};
}
inline bool InsideLaneLabel(float x,float y,unsigned slot,unsigned count=LaneCount) {
    const auto p=LaneLabelCenter(slot,count);
    const auto local=plane::Unproject(x-p.x,y-p.y);
    return std::abs(local.x)<=14.f && std::abs(local.y)<=13.f;
}
inline const wchar_t* LaneFinger(unsigned slot,unsigned count=LaneCount) {
    constexpr const wchar_t* four[]={L"左手中指",L"左手食指",L"右手食指",L"右手中指"};
    constexpr const wchar_t* six[]={L"左手无名指",L"左手中指",L"左手食指",L"右手食指",L"右手中指",L"右手无名指"};
    constexpr const wchar_t* eight[]={L"左手小拇指",L"左手无名指",L"左手中指",L"左手食指",L"右手食指",L"右手中指",L"右手无名指",L"右手小拇指"};
    return count==4?four[slot]:(count<=6?six[slot]:eight[slot]);
}
// Canonical art bits: left ring/middle/index/pinky; right index/middle/ring/pinky.
inline unsigned LeftPose(unsigned mask,unsigned count=LaneCount) {
    if(count==4)return (mask&3u)<<1;
    if(count<=6)return mask&7u;
    return ((mask>>1)&7u)|((mask&1u)<<3);
}
inline unsigned RightPose(unsigned mask,unsigned count=LaneCount) {
    return (mask>>(count==4?2:count<=6?3:4))&(count==4?3u:count==8?15u:7u);
}
}
