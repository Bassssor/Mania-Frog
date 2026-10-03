#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace frog {
struct Point { float x, y; };
struct Particle {
    float x, y, z, vx, vy, vz;
    Point anchor;
    int key;
    float life, radius, age = 0, releaseAge = 0;
    bool star, jet;
};
// Keep the established fountain simulation independent of OBS/graphics.
// It uses the exact fingertip anchors and rates of obs-overlay/particles.js.
struct Fountain {
    std::vector<Particle> particles;
    std::array<float, 4> charge{}, emission{}, strikeAge{{10,10,10,10}};
    std::array<Point, 4> centers{{{710,568},{605,544},{499,523},{397,504}}};
    unsigned previous = 0;
    uint32_t seed = 0x51a7d321;
    float time = 0;
    Fountain() { particles.reserve(2048); }
    float random() {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return (seed & 0xffffff) / 16777216.f;
    }
    static Point anchor(unsigned mask, int key) {
        if (key == 0) return (mask & 3) == 3 ? Point{696,529} : Point{705,533};
        if (key == 1) return (mask & 3) == 3 ? Point{646,517} : Point{594,526};
        if (key == 2) return (mask & 12) == 12 ? Point{484,497} : Point{498,510};
        return (mask & 12) == 12 ? Point{434,491} : Point{413,481};
    }
    void spawn(int key, int count, Point anchor) {
        for (int n = 0; n < count && particles.size() < 2048; ++n) {
            float x = (random()-.5f)*7, y = (random()-.5f)*3.5f;
            float kind = random(); bool star = kind > .78f, jet = kind > .48f && kind <= .78f;
            float factor = jet ? .93f+random()*.12f : star ? .40f+random()*.38f : .18f+random()*.36f;
            float vx = (random()-.5f)*104, vy = (random()-.5f)*70;
            float vz = std::sqrt(1200*std::max(1.f, anchor.y-245)*factor);
            float life = 2.15f+random()*.35f, radius = jet ? 1.1f+random()*.8f : .65f+random()*1.6f;
            particles.push_back({x,y,1,vx,vy,vz,anchor,key,life,radius,0,0,star,jet});
        }
    }
    void advance(float seconds, unsigned held, unsigned pending = 0) {
        float dt = std::clamp(seconds, 0.f, .05f);
        unsigned pressed = (held & ~previous) | pending;
        time += dt;
        for (int i = 0; i < 4; ++i) {
            strikeAge[i] += dt;
            if ((held | pressed) & (1u << i)) centers[i] = anchor(held | pressed, i);
            if (pressed & (1u << i)) {
                spawn(i, 96, centers[i]); charge[i]=1; emission[i]=0; strikeAge[i]=0;
            }
            if (held & (1u << i)) {
                charge[i] = std::max(.58f, charge[i]-dt*2.8f); emission[i] += dt;
                while (emission[i] >= 1/60.f) { spawn(i, 3, centers[i]); emission[i] -= 1/60.f; }
            } else { charge[i] = std::max(0.f, charge[i]-dt*6); emission[i]=0; }
        }
        previous = held;
        size_t kept = 0; float drag = std::exp(-dt*1.1f);
        for (auto p : particles) {
            if (!(held & (1u << p.key)) || p.releaseAge > 0) p.releaseAge += dt;
            p.age += dt; p.x += p.vx*dt; p.y += p.vy*dt; p.z += p.vz*dt;
            p.vx *= drag; p.vy *= drag; p.vz -= 600*dt;
            if (p.age < p.life && p.releaseAge+1e-5f < .40f && !(p.age>.1f && p.z<0)) particles[kept++] = p;
        }
        particles.resize(kept);
    }
    bool active() const {
        return previous || !particles.empty() || std::any_of(charge.begin(), charge.end(), [](float c){return c>0;});
    }
};
}
