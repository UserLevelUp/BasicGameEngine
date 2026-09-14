#pragma once
#include <algorithm>
#include <cmath>
#include "BgeGlassUi.h"

// Pure surface sampling shared by the renderer and its native geometry tests.
// A stylized convex lens, not a simulation of background refraction.
namespace BgeGlassReflection {
inline float Saturate(float x) { return (std::clamp)(x, 0.0f, 1.0f); }
inline float Smooth(float a, float b, float x) {
    const float t = Saturate((x-a)/(b-a)); return t*t*(3-2*t);
}
struct Point { float x, y; };
inline Point Environment(float u, float v) {
    const float x = 2*u-1, y = 2*v-1;
    return {0.5f + 0.5f*x*(0.76f+0.24f*x*x),
            0.5f + 0.5f*y*(0.78f+0.22f*y*y) + 0.23f*x*x};
}
inline float Coverage(float x, float y, float width, float height, float radius) {
    const float r = (std::clamp)(radius, 0.0f, (std::min)(width,height)*0.5f);
    const float qx = std::abs(x-width*0.5f)-(width*0.5f-r);
    const float qy = std::abs(y-height*0.5f)-(height*0.5f-r);
    const float dx = (std::max)(qx,0.0f), dy = (std::max)(qy,0.0f);
    const float distance = std::sqrt(dx*dx+dy*dy)+(std::min)((std::max)(qx,qy),0.0f)-r;
    return Smooth(0.5f, 2.0f, -distance);
}
inline float Intensity(BgeGlassOverlayStyle style, float u, float v) {
    const auto p = Environment(u,v);
    float light = 0;
    if (style == BgeGlassOverlayStyle::WindowFourPane) {
        const float inside = Smooth(0.05f,0.07f,p.x)*(1-Smooth(0.80f,0.82f,p.x))
            *Smooth(0.04f,0.06f,p.y)*(1-Smooth(0.64f,0.67f,p.y));
        light = inside*Smooth(0.012f,0.026f,std::abs(p.x-0.43f))
            *Smooth(0.017f,0.036f,std::abs(p.y-0.32f));
    } else if (style == BgeGlassOverlayStyle::CloverFourLeaf) {
        const float x = (p.x-0.26f)*1.8f, y = p.y-0.30f;
        const float offset = 0.095f, radius = 0.135f;
        const float d = (std::min)((std::min)(std::hypot(x-offset,y),std::hypot(x+offset,y)),
            (std::min)(std::hypot(x,y-offset),std::hypot(x,y+offset)));
        light = 1-Smooth(radius-0.014f,radius+0.014f,d);
    } else if (style == BgeGlassOverlayStyle::SpecularSheen) {
        light = std::exp(-std::pow((p.y-0.18f-0.10f*p.x)/0.12f,2.0f));
    } else if (style == BgeGlassOverlayStyle::FrostedDiffuse) {
        light = 0.32f;
    }
    return Saturate(light*(0.85f-0.45f*v)*Smooth(0.0f,0.09f,u)*(1-Smooth(0.91f,1.0f,u)));
}
}
