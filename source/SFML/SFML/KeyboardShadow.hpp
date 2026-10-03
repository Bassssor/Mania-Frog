#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace KeyboardShadow {
constexpr int Left=300,Top=495,Width=520,Height=220;
inline std::vector<float> Blur(const std::vector<float>& mask,float sigma) {
    const int radius=static_cast<int>(std::ceil(sigma*3.f));
    std::vector<float> kernel(radius*2+1);float sum=0.f;
    for(int i=-radius;i<=radius;++i){const float value=std::exp(-i*i/(2.f*sigma*sigma));kernel[i+radius]=value;sum+=value;}
    for(auto& value:kernel)value/=sum;
    std::vector<float> horizontal(mask.size()),result(mask.size());
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)for(int i=-radius;i<=radius;++i) {
        if(x+i>=0 && x+i<Width)horizontal[y*Width+x]+=mask[y*Width+x+i]*kernel[i+radius];
    }
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)for(int i=-radius;i<=radius;++i) {
        if(y+i>=0 && y+i<Height)result[y*Width+x]+=horizontal[(y+i)*Width+x]*kernel[i+radius];
    }
    return result;
}
inline sf::Image Build(const sf::Image& keyboard,const sf::Image& corner,unsigned canvasWidth,unsigned canvasHeight) {
    std::vector<float> footprint(Width*Height);
    for(int x=320;x<800;++x) {
        int bottom=-1;
        // The chassis' real lower silhouette, including its repaired corner,
        // anchors contact. Elevated keycap outlines never enlarge the footprint.
        for(int y=520;y<669;++y)
            if(std::max(keyboard.getPixel(x,y).a,corner.getPixel(x,y).a)>=32)bottom=y;
        if(bottom<0)continue;
        const float rear=std::max(.18f*x+454.08f,-.85f*x+859.5f);
        for(int y=std::max(Top,static_cast<int>(std::floor(rear)));y<=bottom && y<Top+Height;++y) {
            const float rearCoverage=std::clamp(y+.5f-rear,0.f,1.f);
            float frontCoverage=1.f;
            if(y==bottom)frontCoverage=std::max(keyboard.getPixel(x,y).a,corner.getPixel(x,y).a)/255.f;
            footprint[(y-Top)*Width+x-Left]=rearCoverage*frontCoverage;
        }
    }
    const auto contact=Blur(footprint,1.2f),cast=Blur(footprint,3.2f);
    sf::Image result;result.create(canvasWidth,canvasHeight,sf::Color::Transparent);
    auto sample=[](const std::vector<float>& field,int x,int y) {
        return x>=0 && x<Width && y>=0 && y<Height?field[y*Width+x]:0.f;
    };
    for(int y=Top;y<Top+Height;++y)for(int x=Left;x<Left+Width;++x) {
        const float close=.32f*sample(contact,x-Left,y-Top-1);
        // The sprite is lit from above-left: the short cast extends right/down.
        const float soft=.20f*sample(cast,x-Left-7,y-Top-5);
        const auto alpha=static_cast<sf::Uint8>(std::lround((1.f-(1.f-close)*(1.f-soft))*255.f));
        if(alpha)result.setPixel(x,y,sf::Color(68,54,35,alpha));
    }
    return result;
}
}
