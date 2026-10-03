#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ArmShadow {
constexpr int Left=300,Top=200,Width=650,Height=420;
inline std::vector<float> Blur(const std::vector<float>& mask,float sigma) {
    const int radius=static_cast<int>(std::ceil(sigma*3.f));
    std::vector<float> kernel(radius*2+1);float sum=0.f;
    for(int i=-radius;i<=radius;++i){const float value=std::exp(-i*i/(2.f*sigma*sigma));kernel[i+radius]=value;sum+=value;}
    for(auto& value:kernel)value/=sum;
    std::vector<float> horizontal(mask.size()),result(mask.size());
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)for(int i=-radius;i<=radius;++i)
        if(x+i>=0 && x+i<Width)horizontal[y*Width+x]+=mask[y*Width+x+i]*kernel[i+radius];
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)for(int i=-radius;i<=radius;++i)
        if(y+i>=0 && y+i<Height)result[y*Width+x]+=horizontal[(y+i)*Width+x]*kernel[i+radius];
    return result;
}
// Use the rendered pose, including the inward-reaching F/J mesh. The body
// receives the shadow; keycaps, tabletop and transparent background do not.
inline sf::Image Build(const sf::Image& pose,const sf::Image& body,const sf::Image& keyboard) {
    std::vector<float> mask(Width*Height);
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x) {
        const auto pixel=pose.getPixel(x+Left,y+Top);
        if(!pixel.a)continue;
        // The upper arm merges into the torso: it has no separation from
        // that receiver and must not cast the old diagonal shoulder stripe.
        // Suspended fingers cast fully; the yellow forearm's separation
        // increases smoothly toward its lower, protruding part.
        const float finger=std::clamp((70.f-(pixel.g-pixel.b)*255.f/pixel.a)/35.f,0.f,1.f);
        const float t=std::clamp((y+Top-295.f)/50.f,0.f,1.f);
        const float separation=t*t*(3.f-2.f*t);
        mask[y*Width+x]=pixel.a/255.f*std::max(finger,separation);
    }
    const auto contact=Blur(mask,3.f),ambient=Blur(mask,8.f),cast=Blur(mask,6.f);
    auto sample=[](const std::vector<float>& field,int x,int y) {
        return x>=0 && x<Width && y>=0 && y<Height?field[y*Width+x]:0.f;
    };
    sf::Image result;result.create(Width,Height,sf::Color::Transparent);
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x) {
        const float receiver=body.getPixel(x+Left,y+Top).a/255.f *
            (1.f-keyboard.getPixel(x+Left,y+Top).a/255.f);
        // Strong warm contact shading along the arm/body join, with a wider
        // ambient falloff and a subtler above-left light cast. No hard stroke.
        const float close=.34f*sample(contact,x-3,y-5);
        const float ambientOcclusion=.19f*sample(ambient,x-5,y-7);
        const float soft=.16f*sample(cast,x-8,y-10);
        const auto alpha=static_cast<sf::Uint8>(std::lround(
            (1.f-(1.f-close)*(1.f-ambientOcclusion)*(1.f-soft))*receiver*255.f));
        // Premultiplied RGB supports multiplying the receiver's color while
        // retaining its exact alpha, including antialiased contour pixels.
        if(alpha)result.setPixel(x,y,sf::Color(
            static_cast<sf::Uint8>((116*alpha+127)/255),
            static_cast<sf::Uint8>((72*alpha+127)/255),
            static_cast<sf::Uint8>((25*alpha+127)/255),alpha));
    }
    return result;
}
}
