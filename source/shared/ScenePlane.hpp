#pragma once

// One orthographic camera for the table, keyboard surfaces and printed legends.
namespace frog::plane {
constexpr float UX=1.f,UY=.18f,VX=-.75f,VY=.6375f;
struct Point {float x,y;};
inline Point Project(float u,float v) {return {UX*u+VX*v,UY*u+VY*v};}
inline Point Unproject(float x,float y) {
    constexpr float determinant=UX*VY-VX*UY;
    return {(VY*x-VX*y)/determinant,(-UY*x+UX*y)/determinant};
}
}
