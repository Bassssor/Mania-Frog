#pragma once
#include "DeskSurface.hpp"
#include "../../shared/LaneLayout.hpp"

namespace KeyboardSurface {
struct Ring {std::vector<DeskSurface::Point> points;float height;};
inline Ring Outline(float width,float depth,float radius,float height) {
    Ring result;result.height=height;
    const float w=width/2-radius,d=depth/2-radius;
    const std::array<sf::Vector2f,4> centers={sf::Vector2f(w,-d),sf::Vector2f(w,d),sf::Vector2f(-w,d),sf::Vector2f(-w,-d)};
    for(unsigned corner=0;corner<4;++corner)for(unsigned step=0;step<=16;++step) {
        const float angle=(-90.f+90.f*corner+90.f*step/16)*3.14159265359f/180.f;
        const sf::Vector2f normal(std::cos(angle),std::sin(angle));
        result.points.push_back({centers[corner]+normal*radius,normal});
    }
    return result;
}
inline sf::Vector2f Project(sf::Vector2f center,sf::Vector2f local,float height) {
    const auto p=frog::plane::Project(local.x,local.y);
    return {center.x+p.x,center.y+p.y+height};
}
inline void Wall(sf::RenderTarget& target,sf::Vector2f center,const Ring& top,const Ring& bottom,
                 sf::Color color,float contrast) {
    sf::VertexArray mesh(sf::Triangles);
    for(unsigned i=0;i<top.points.size();++i) {
        const unsigned j=(i+1)%unsigned(top.points.size());
        auto shade=[&](sf::Vector2f n){return DeskSurface::Shade(color,1.f+contrast*(-.55f*n.x-.35f*n.y));};
        const auto a=shade(top.points[i].normal),b=shade(top.points[j].normal);
        DeskSurface::Quad(mesh,Project(center,top.points[i].plane,top.height),Project(center,top.points[j].plane,top.height),
            Project(center,bottom.points[j].plane,bottom.height),Project(center,bottom.points[i].plane,bottom.height),
            a,b,DeskSurface::Shade(b,.93f),DeskSurface::Shade(a,.93f));
    }
    target.draw(mesh);
}
inline void Face(sf::RenderTarget& target,sf::Vector2f center,const Ring& edge,sf::Color color,float middleHeight) {
    sf::VertexArray mesh(sf::Triangles);
    for(unsigned i=0;i<edge.points.size();++i) {
        const unsigned j=(i+1)%unsigned(edge.points.size());
        mesh.append(sf::Vertex(Project(center,{0.f,0.f},middleHeight),color));
        for(unsigned k:{i,j}) {
            const auto& p=edge.points[k];
            mesh.append(sf::Vertex(Project(center,p.plane,edge.height),
                DeskSurface::Shade(color,1.f+.015f*(-p.normal.x-p.normal.y))));
        }
    }
    target.draw(mesh);
}
inline void Draw(sf::RenderTarget& target,unsigned count) {
    std::vector<sf::Vector2f> keys;
    for(unsigned i=0;i<count;++i) {
        const auto p=frog::LaneKeyOrigin(i,count);keys.push_back({p.x,p.y});
    }
    std::sort(keys.begin(),keys.end(),[](auto a,auto b){return a.x<b.x;});
    const auto middle=(keys.front()+keys.back())*.5f;
    const auto forward=frog::plane::Project(0.f,11.f);
    const sf::Vector2f center=middle+sf::Vector2f(forward.x,forward.y);
    const float width=keys.back().x-keys.front().x+51.f;
    // The low, continuous case joins both key banks into one keyboard.
    const auto lower=Outline(width,77.f,12.f,15.f),rim=Outline(width,77.f,12.f,10.f);
    const auto deck=Outline(width-3.f,74.f,10.5f,8.f);
    Wall(target,center,rim,lower,sf::Color(199,194,183),.055f);
    Wall(target,center,deck,rim,sf::Color(239,235,224),.025f);
    Face(target,center,deck,sf::Color(220,218,207),8.f);
    // Recessed wells and a narrow rim replace the previous flat cutout blocks.
    for(unsigned half=0;half<2;++half) {
        const unsigned first=half*count/2,last=first+count/2-1;
        const auto anchor=(keys[first]+keys[last])*.5f+sf::Vector2f(forward.x,forward.y);
        const float span=keys[last].x-keys[first].x+39.f;
        Face(target,anchor,Outline(span,63.f,7.f,7.5f),sf::Color(126,133,125),7.5f);
        Face(target,anchor+sf::Vector2f(0,1),Outline(span-2,61.f,6.f,7.5f),sf::Color(153,158,148),7.5f);
    }
    for(auto anchor:keys) {
        anchor+=sf::Vector2f(forward.x,forward.y);
        const auto base=Outline(31.5f,56.f,6.f,7.f);
        const auto shoulder=Outline(31.5f,55.f,6.f,3.f);
        const auto top=Outline(29.5f,52.f,5.f,0.f);
        const auto dish=Outline(26.5f,48.f,4.f,.65f);
        Wall(target,anchor,shoulder,base,sf::Color(211,205,192),.08f);
        Wall(target,anchor,top,shoulder,sf::Color(248,244,230),.025f);
        Wall(target,anchor,dish,top,sf::Color(249,246,235),.015f);
        Face(target,anchor,dish,sf::Color(242,239,226),.9f);
    }
}
inline bool Render(sf::RenderTexture& output,unsigned width,unsigned height,unsigned count) {
    sf::RenderTexture detail;
    if(!detail.create(width*3,height*3)||!output.create(width,height))return false;
    detail.setView(sf::View(sf::FloatRect(0,0,float(width),float(height))));
    detail.clear(sf::Color::Transparent);Draw(detail,count);detail.display();detail.setSmooth(true);
    sf::Sprite sprite(detail.getTexture());sprite.setScale(1.f/3,1.f/3);
    const sf::BlendMode premultiplied(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add,
        sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
    output.clear(sf::Color::Transparent);output.draw(sprite,sf::RenderStates(premultiplied));output.display();
    return true;
}
}
