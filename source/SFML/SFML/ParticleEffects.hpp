#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Native particles, independent of the stationary artwork and keycaps.
class ParticleEffects {
    struct Particle {
        sf::Vector3f position, velocity;
        sf::Vector2f anchor;
        float age, life, radius, releaseAge;
        unsigned key;
        bool star,jet;
    };
    std::vector<Particle> particles;
    sf::Texture halo;
    std::array<float, 4> charge{}, emission{};
    std::array<float, 4> strikeAge{{10.f,10.f,10.f,10.f}};
    unsigned previous = 0;
    std::uint32_t seed = 0x51A7D321u;
    float time = 0.f;
    std::array<sf::Vector2f,4> strikeCenters{};
    mutable std::vector<sf::Vertex> glowVertices,solidVertices,trailVertices;
    float Random() {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return static_cast<float>(seed & 0xffffffu) / 16777216.f;
    }
    void Spawn(unsigned key, unsigned count,sf::Vector2f anchor) {
        for (unsigned n = 0; n < count && particles.size() < MaxParticles; ++n) {
            const float along = (Random() - .5f) * 7.f;
            const float depth = (Random() - .5f) * 3.5f;
            Particle p{};
            p.position = {along,depth,1.f};p.anchor=anchor;
            const float kind=Random();p.star=kind>.78f;p.jet=kind>.48f && kind<=.78f;
            // Aim the tall streams at the same neck-height band for all four
            // contacts, despite the keyboard's slanted perspective.
            const float height=std::max(1.f,anchor.y-245.f);
            const float heightFactor=p.jet ? .93f+Random()*.12f
                : p.star ? .40f+Random()*.38f : .18f+Random()*.36f;
            p.velocity = { (Random()-.5f)*104.f, (Random()-.5f)*70.f,
                std::sqrt(2.f*Gravity*height*heightFactor) };
            p.life = 2.15f + Random() * .35f;
            p.radius = p.jet ? 1.1f+Random()*.8f : .65f+Random()*1.6f;
            p.key = key;
            particles.push_back(p);
        }
    }
    void Glow(sf::RenderTarget& target, sf::Vector2f position, sf::Vector2f radii,
        sf::Color color, float rotation = 0.f) const {
        sf::Sprite sprite(halo);
        sprite.setOrigin(32.f, 32.f); sprite.setPosition(position);
        sprite.setScale(radii.x / 32.f, radii.y / 32.f);
        sprite.setRotation(rotation); sprite.setColor(color);
        target.draw(sprite);
    }
public:
    static constexpr float Gravity=600.f;
    static constexpr float ReleaseFadeSeconds=.40f;
    static constexpr unsigned BurstCount = 96, MaxParticles = 2048;
    inline static const std::array<sf::Vector2f, 4> Centers = {{
        {710.f,568.f}, {605.f,544.f}, {499.f,523.f}, {397.f,504.f}
    }};
    static sf::Vector2f Anchor(unsigned mask,unsigned key) {
        // Measured fingertip contacts, in the registered native canvas.
        // The combined poses move a finger slightly, so their anchors differ.
        if(key==0)return (mask&3u)==3u?sf::Vector2f(696,529):sf::Vector2f(705,533);
        if(key==1)return (mask&3u)==3u?sf::Vector2f(646,517):sf::Vector2f(594,526);
        if(key==2)return (mask&12u)==12u?sf::Vector2f(484,497):sf::Vector2f(498,510);
        return (mask&12u)==12u?sf::Vector2f(434,491):sf::Vector2f(413,481);
    }
    bool Load() {
        particles.reserve(MaxParticles);
        glowVertices.reserve(MaxParticles*6);
        solidVertices.reserve(MaxParticles*60);
        trailVertices.reserve(MaxParticles*2);
        sf::Image image; image.create(64,64,sf::Color::Transparent);
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
            const float dx = (x + .5f - 32.f) / 32.f, dy = (y + .5f - 32.f) / 32.f;
            const float r2 = dx*dx + dy*dy;
            // Smooth falloff to actual zero, without a square or hard rim.
            const float a = r2 < 1.f ? std::exp(-r2 * 5.f) * (1.f - r2) : 0.f;
            image.setPixel(x,y,sf::Color(255,255,255,static_cast<sf::Uint8>(a*255.f)));
        }
        const bool ok = halo.loadFromImage(image); halo.setSmooth(true); return ok;
    }
    void Reset() {
        particles.clear(); charge.fill(0.f); emission.fill(0.f);
        strikeAge.fill(10.f);
        previous = 0; seed = 0x51A7D321u; time = 0.f;
        strikeCenters=Centers;
    }
    void Advance(float seconds, unsigned held, unsigned pending = 0) {
        const float dt = std::clamp(seconds, 0.f, .05f); time += dt;
        const unsigned pressed = (held & ~previous) | pending;
        for (unsigned i = 0; i < 4; ++i) {
            strikeAge[i] += dt;
            if((held|pressed)&(1u<<i))strikeCenters[i]=Anchor(held|pressed,i);
            if (pressed & (1u << i)) { Spawn(i,BurstCount,strikeCenters[i]); charge[i] = 1.f; emission[i] = 0.f; strikeAge[i]=0.f; }
            if (held & (1u << i)) {
                charge[i] = std::max(.58f, charge[i] - dt * 2.8f);
                emission[i] += dt;
                constexpr float emissionInterval=1.f/60.f;
                while (emission[i] >= emissionInterval) { Spawn(i,3,strikeCenters[i]); emission[i] -= emissionInterval; }
            } else { charge[i] = std::max(0.f, charge[i] - dt * 6.f); emission[i] = 0.f; }
        }
        previous = held;
        for (auto& p : particles) {
            // A released key's existing sparks fade independently. Repressing
            // it emits fresh sparks without reviving the previous trail.
            if(!(held&(1u<<p.key)) || p.releaseAge>0.f)p.releaseAge+=dt;
            p.age += dt; p.position += p.velocity * dt;
            p.velocity.x *= std::exp(-dt * 1.1f);
            p.velocity.y *= std::exp(-dt * 1.1f);
            p.velocity.z -= Gravity * dt;
        }
        particles.erase(std::remove_if(particles.begin(),particles.end(),
            [](const Particle& p) { return p.age >= p.life || p.releaseAge+1.e-5f>=ReleaseFadeSeconds ||
                (p.age>.1f && p.position.z<0.f); }),particles.end());
    }
    bool Active() const {
        return previous != 0 || !particles.empty() ||
            std::any_of(charge.begin(),charge.end(),[](float x){ return x > 0.f; });
    }
    unsigned Count(unsigned key) const {
        return static_cast<unsigned>(std::count_if(particles.begin(),particles.end(),
            [key](const Particle& p){ return p.key == key; }));
    }
    unsigned Count() const { return static_cast<unsigned>(particles.size()); }
    void DrawSurface(sf::RenderTarget& target) const {
        for (unsigned i = 0; i < 4; ++i) if (charge[i] > 0.f) {
            const float pulse = .94f + .06f * std::sin(time * 9.f + i);
            Glow(target,strikeCenters[i],{38.f,16.f},sf::Color(255,200,65,
                static_cast<sf::Uint8>(charge[i]*pulse*150.f)),10.f);
            Glow(target,strikeCenters[i],{12.f,5.f},
                sf::Color(255,243,183,static_cast<sf::Uint8>(charge[i]*85.f)),10.f);
        }
        for (unsigned i=0;i<4;++i) if (strikeAge[i]<.30f) {
            const float phase=strikeAge[i]/.30f, radius=7.f+phase*37.f;
            const sf::Color color(255,231,132,static_cast<sf::Uint8>((1.f-phase)*155.f));
            sf::VertexArray ring(sf::TriangleStrip,130);
            for (unsigned point=0;point<=64;++point) {
                const float angle=point*6.2831853f/64.f;
                for (unsigned edge=0;edge<2;++edge) {
                    const float r=radius+edge*1.4f;
                    const float x=std::cos(angle)*r,y=std::sin(angle)*r*.37f;
                    ring[point*2+edge]=sf::Vertex(strikeCenters[i]+sf::Vector2f(x-y*.5f,x*.16f+y),color);
                }
            }
            target.draw(ring);
        }
    }
    void DrawParticles(sf::RenderTarget& target, int depthPass = 0) const {
        glowVertices.clear();solidVertices.clear();trailVertices.clear();
        for (const auto& p : particles) {
            if (depthPass<0 && p.position.y>=0.f) continue;
            if (depthPass>0 && p.position.y<0.f) continue;
            // Project a 3D trajectory onto the oblique keyboard plane. Depth
            // affects overlap, size and brightness; height lifts the spark.
            const sf::Vector2f position=p.anchor+sf::Vector2f(
                p.position.x-p.position.y*.58f,p.position.x*.16f+p.position.y*.85f-p.position.z);
            const float depthScale=std::clamp(1.f+p.position.y*.008f,.70f,1.30f);
            const float radius=p.radius*depthScale;
            const float release=std::clamp(1.f-p.releaseAge/ReleaseFadeSeconds,0.f,1.f);
            const float fade = std::pow(1.f - p.age / p.life,.85f)*release*release*(3.f-2.f*release);
            const float appear = std::min(1.f,p.age * 100.f + .2f);
            const auto alpha = static_cast<sf::Uint8>(std::min(255.f,240.f*fade*appear*depthScale));
            const float glowRadius=radius*4.5f;
            const sf::Color glowColor(255,193,48,static_cast<sf::Uint8>(alpha*.76f));
            const std::array<sf::Vector2f,4> corners={{{-glowRadius,-glowRadius},
                {glowRadius,-glowRadius},{glowRadius,glowRadius},{-glowRadius,glowRadius}}};
            const std::array<sf::Vector2f,4> uv={{{0,0},{64,0},{64,64},{0,64}}};
            for(unsigned corner : {0u,1u,2u,0u,2u,3u})
                glowVertices.emplace_back(position+corners[corner],glowColor,uv[corner]);
            const float coreRadius=radius*(.7f+fade*.3f);
            const sf::Color coreColor(255,244,182,alpha);
            for(unsigned point=0;point<12;++point) {
                const float a=point*6.2831853f/12.f,b=(point+1)*6.2831853f/12.f;
                solidVertices.emplace_back(position,coreColor);
                solidVertices.emplace_back(position+sf::Vector2f(std::sin(a),-std::cos(a))*coreRadius,coreColor);
                solidVertices.emplace_back(position+sf::Vector2f(std::sin(b),-std::cos(b))*coreRadius,coreColor);
            }
            if (p.star) {
                // A fine four-point glint; no large sticker-like star.
                const float r = radius*(1.5f + fade), waist = radius*.25f;
                const std::array<sf::Vector2f,8> points = {{{0,-r},{waist,-waist},{r,0},
                    {waist,waist},{0,r},{-waist,waist},{-r,0},{-waist,-waist}}};
                const sf::Color starColor(255,251,224,alpha);
                for(unsigned i=0;i<8;++i) {
                    solidVertices.emplace_back(position,starColor);
                    solidVertices.emplace_back(position+points[i],starColor);
                    solidVertices.emplace_back(position+points[(i+1)%8],starColor);
                }
            } else if (p.age > .018f) {
                const sf::Vector2f projectedVelocity(p.velocity.x-p.velocity.y*.58f,
                    p.velocity.x*.16f+p.velocity.y*.85f-p.velocity.z);
                trailVertices.emplace_back(position,sf::Color(255,224,110,alpha/2));
                trailVertices.emplace_back(position-projectedVelocity*(p.jet?.065f:.035f),sf::Color(255,173,32,0));
            }
        }
        // At most three GPU submissions per depth pass, regardless of spark
        // count. Retain both depth passes so the hand still occludes sparks.
        if(!glowVertices.empty())target.draw(glowVertices.data(),glowVertices.size(),sf::Triangles,sf::RenderStates(&halo));
        if(!solidVertices.empty())target.draw(solidVertices.data(),solidVertices.size(),sf::Triangles);
        if(!trailVertices.empty())target.draw(trailVertices.data(),trailVertices.size(),sf::Lines);
    }
};
