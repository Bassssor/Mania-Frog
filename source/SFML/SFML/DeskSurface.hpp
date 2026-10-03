#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Dimensions and corner radii are in one table plane. Every corner uses the
// same quarter circle before projection; the keyboard determines both axes.
namespace DeskSurface {
constexpr float Width=650.f,Depth=250.f,Radius=30.f,CenterX=560.f,CenterY=564.f;
constexpr float Thickness=14.f;
struct Point {sf::Vector2f plane,normal;};
inline sf::Vector2f Project(sf::Vector2f p,float height=0.f) {
    return {CenterX+p.x-.75f*p.y,CenterY+.18f*p.x+.6375f*p.y+height};
}
inline std::vector<Point> Outline(float inset=0.f) {
    const float w=Width/2.f-inset,d=Depth/2.f-inset,r=Radius-inset;
    const std::array<sf::Vector2f,4> centers={sf::Vector2f(w-r,-d+r),
        sf::Vector2f(w-r,d-r),sf::Vector2f(-w+r,d-r),sf::Vector2f(-w+r,-d+r)};
    std::vector<Point> points;points.reserve(100);
    for(unsigned corner=0;corner<4;++corner)for(unsigned step=0;step<=24;++step) {
        const float angle=(-90.f+90.f*corner+90.f*step/24.f)*3.14159265359f/180.f;
        const sf::Vector2f normal(std::cos(angle),std::sin(angle));
        points.push_back({centers[corner]+normal*r,normal});
    }
    return points;
}
inline sf::Color Shade(sf::Color base,float factor) {
    auto c=[factor](sf::Uint8 value){return static_cast<sf::Uint8>(std::clamp(value*factor,0.f,255.f));};
    return {c(base.r),c(base.g),c(base.b),255};
}
inline void Quad(sf::VertexArray& mesh,sf::Vector2f a,sf::Vector2f b,sf::Vector2f c,sf::Vector2f d,
    sf::Color ca,sf::Color cb,sf::Color cc,sf::Color cd) {
    for(const auto& v:std::array<sf::Vertex,6>{sf::Vertex(a,ca),sf::Vertex(b,cb),sf::Vertex(c,cc),
        sf::Vertex(a,ca),sf::Vertex(c,cc),sf::Vertex(d,cd)})mesh.append(v);
}
inline void Draw(sf::RenderTarget& target,bool topOnly=false) {
    const auto outer=Outline(),inner=Outline(2.f);
    if(topOnly) {
        sf::VertexArray silhouette(sf::Triangles);
        for(unsigned i=0;i<outer.size();++i) {
            silhouette.append(sf::Vertex(Project({0.f,0.f}),sf::Color::White));
            silhouette.append(sf::Vertex(Project(outer[i].plane),sf::Color::White));
            silhouette.append(sf::Vertex(Project(outer[(i+1)%outer.size()].plane),sf::Color::White));
        }
        target.draw(silhouette);return;
    }
    sf::VertexArray sides(sf::Triangles),bevel(sf::Triangles),top(sf::Triangles);
    for(unsigned i=0;i<outer.size();++i) {
        const unsigned next=(i+1)%static_cast<unsigned>(outer.size());
        auto sideShade=[](sf::Vector2f n){return 1.f+.025f*n.y-.035f*n.x;};
        const auto ca=Shade(sf::Color(229,220,207),sideShade(outer[i].normal));
        const auto cb=Shade(sf::Color(229,220,207),sideShade(outer[next].normal));
        Quad(sides,Project(outer[i].plane,2.f),Project(outer[next].plane,2.f),
            Project(outer[next].plane,Thickness),Project(outer[i].plane,Thickness),
            ca,cb,Shade(cb,.965f),Shade(ca,.965f));
        Quad(bevel,Project(inner[i].plane),Project(inner[next].plane),Project(outer[next].plane,2.f),
            Project(outer[i].plane,2.f),sf::Color(252,248,241),sf::Color(252,248,241),ca,cb);
        auto topShade=[](sf::Vector2f p){return 1.f-.009f*p.x/(Width/2.f)-.009f*p.y/(Depth/2.f);};
        top.append(sf::Vertex(Project({0.f,0.f}),sf::Color(245,240,233)));
        top.append(sf::Vertex(Project(inner[i].plane),Shade(sf::Color(245,240,233),topShade(inner[i].plane))));
        top.append(sf::Vertex(Project(inner[next].plane),Shade(sf::Color(245,240,233),topShade(inner[next].plane))));
    }
    if(!topOnly)target.draw(sides);
    target.draw(bevel);target.draw(top);
}
// Geometry is rendered once at 3x resolution, then sampled with premultiplied
// alpha. Runtime still draws one cached texture, including the bevel.
inline bool Render(sf::RenderTexture& output,unsigned width,unsigned height,bool topOnly=false) {
    sf::RenderTexture detail;
    if(!detail.create(width*3,height*3) || !output.create(width,height))return false;
    detail.setView(sf::View(sf::FloatRect(0.f,0.f,static_cast<float>(width),static_cast<float>(height))));
    detail.clear(sf::Color::Transparent);Draw(detail,topOnly);detail.display();detail.setSmooth(true);
    sf::Sprite sprite(detail.getTexture());sprite.setScale(1.f/3.f,1.f/3.f);
    const sf::BlendMode premultiplied(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,
        sf::BlendMode::Add,sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
    output.clear(sf::Color::Transparent);output.draw(sprite,sf::RenderStates(premultiplied));output.display();
    return true;
}
}
