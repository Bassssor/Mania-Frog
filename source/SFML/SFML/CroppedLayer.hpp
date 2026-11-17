#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>

// Store only visible pixels, retaining transparent padding for linear filtering.
struct CroppedLayer {
    sf::Texture texture;
    sf::Vector2f origin;
    bool Load(const sf::Image& image,sf::IntRect cell,sf::Vector2f offset={}) {
        int left=cell.left+cell.width,top=cell.top+cell.height;
        int right=cell.left,bottom=cell.top;
        const auto* rgba=image.getPixelsPtr();
        const auto stride=image.getSize().x;
        for(int y=cell.top;y<cell.top+cell.height;++y)for(int x=cell.left;x<cell.left+cell.width;++x)
            if(rgba[(size_t(y)*stride+x)*4+3]) {
                left=std::min(left,x);top=std::min(top,y);
                right=std::max(right,x+1);bottom=std::max(bottom,y+1);
            }
        if(right<=left || bottom<=top){left=cell.left;top=cell.top;right=left+1;bottom=top+1;}
        left=std::max(cell.left,left-2);top=std::max(cell.top,top-2);
        right=std::min(cell.left+cell.width,right+2);bottom=std::min(cell.top+cell.height,bottom+2);
        origin=offset+sf::Vector2f(float(left-cell.left),float(top-cell.top));
        if(!texture.loadFromImage(image,sf::IntRect(left,top,right-left,bottom-top)))return false;
        texture.setSmooth(true);return true;
    }
    sf::Sprite Sprite()const {
        sf::Sprite sprite(texture);sprite.setPosition(origin);return sprite;
    }
};
