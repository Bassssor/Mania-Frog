#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace KeyboardRear {
inline float CornerClosureCoverage(int x,int y) {
    if(x<756 || x>770 || y<537 || y>551)return 0.f;
    // The small triangular opening at D's right-rear corner is sealed with
    // the adjacent chassis deck. Keep the cap and its lower side groove.
    static const std::array<sf::Vector2f,5> outline={{{758.f,538.f},{769.f,538.f},
        {765.f,545.f},{760.f,550.f},{757.5f,549.f}}};
    unsigned covered=0;
    for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
        const float px=x+(sx+.5f)/4.f,py=y+(sy+.5f)/4.f;
        bool inside=false;
        for(unsigned i=0,j=static_cast<unsigned>(outline.size()-1);i<outline.size();j=i++) {
            const auto a=outline[i],b=outline[j];
            if((a.y>py)!=(b.y>py) && px<(b.x-a.x)*(py-a.y)/(b.y-a.y)+a.x)inside=!inside;
        }
        covered+=inside;
    }
    return covered/16.f;
}
inline sf::Image Build(const sf::Image& original) {
    sf::Image result;result.create(original.getSize().x,original.getSize().y,sf::Color::Transparent);
    // Complete the chassis behind and between the elevated keycaps. Its rear
    // perimeter meets their back corners; the original caps and side grooves
    // occlude it naturally. There are no added strips above the cap surfaces.
    for(int y=466;y<564;++y)for(int x=382;x<791;++x) {
        float alpha=0.f,red=0.f,green=0.f,blue=0.f;
        for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
            const float px=x+(sx+.5f)/4.f,py=y+(sy+.5f)/4.f;
            const float dx=px-407.f,dy=py-471.f;
            const float u=(17.f*dx+19.f*dy)/7735.f;
            const float v=(-68.f*dx+379.f*dy)/7735.f;
            if(u<0.f || u>1.f || v<0.f || v>1.f)continue;
            float r=238.f-12.f*v-2.f*u,g=232.f-10.f*v-2.f*u,b=226.f-9.f*v-2.f*u;
            if(v<.12f){r+=8.f;g+=8.f;b+=8.f;}
            alpha+=1.f;red+=r;green+=g;blue+=b;
        }
        if(alpha)result.setPixel(x,y,sf::Color(static_cast<sf::Uint8>(std::lround(red/alpha)),
            static_cast<sf::Uint8>(std::lround(green/alpha)),static_cast<sf::Uint8>(std::lround(blue/alpha)),
            static_cast<sf::Uint8>(std::lround(alpha/16.f*255.f))));
    }
    return result;
}
}
