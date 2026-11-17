#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>

struct ExitAnimation {
    static constexpr double HoldSeconds=.6, FadeSeconds=.8;
    bool active=false;
    uint64_t started=0;
    void Begin(uint64_t now) {
        if(active)return;
        active=true;started=now;
    }
    double Elapsed(uint64_t now) const {return now>=started?double(now-started)/1e9:0.;}
    unsigned char Opacity(uint64_t now) const {
        if(!active)return 255;
        const double t=std::clamp((Elapsed(now)-HoldSeconds)/FadeSeconds,0.,1.);
        return static_cast<unsigned char>(std::lround(255.*(1.-t*t*(3.-2.*t))));
    }
    bool Finished(uint64_t now) const {return active && Elapsed(now)>=HoldSeconds+FadeSeconds;}
};
