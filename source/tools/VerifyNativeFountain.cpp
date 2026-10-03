#include "../obs-native/Fountain.hpp"
#include <iostream>
#include <stdexcept>

void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
void advance(frog::Fountain &f,float seconds,int fps,unsigned held,unsigned pending=0) {
    float remaining=seconds;bool first=true;
    while(remaining>1e-6f) {
        float dt=std::min(1.f/fps,remaining);float sub=dt;
        do {float step=std::min(1/120.f,sub);f.advance(step,held,first?pending:0);first=false;sub-=step;} while(sub>1e-6f);
        remaining-=dt;
    }
}
int main() {
    try {
        for(int fps:{30,60,120,240}) for(unsigned mask=1;mask<16;++mask) {
            frog::Fountain f;float highest=880;
            for(int frame=0;frame<fps*2;++frame) {
                advance(f,1.f/fps,fps,mask);
                require(f.particles.size()<=2048,"Particle budget exceeded");
                for(const auto& p:f.particles)highest=std::min(highest,p.anchor.y+p.x*.16f+p.y*.85f-p.z);
            }
            require(highest>=190 && highest<=280,"Fountain did not reach neck height");
            advance(f,.42f,fps,0);
            require(f.particles.empty() && !f.active(),"Released fountain did not clear within .42s");
        }
        frog::Fountain f;advance(f,.7f,120,15);advance(f,.45f,120,14);
        require(!f.particles.empty(),"Held keys stopped when another key released");
        for(const auto& p:f.particles)require(p.key!=0,"Released D particles survived past fade limit");
        frog::Fountain tap;advance(tap,1/120.f,120,0,1);
        require(!tap.particles.empty() && tap.previous==0,"Subframe tap was lost or left a stuck held pose");
        advance(tap,.42f,120,0);require(tap.particles.empty(),"Subframe tap particles never cleared");
        std::cout<<"PASS: 60 mask/FPS cases, neck-height fountain, bounded particles, per-key release and subframe taps.\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
