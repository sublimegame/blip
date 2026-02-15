//
//  VXColor.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/19/20.
//

#include "VXColor.hpp"

// C++
#include <cmath>

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include <imgui/bgfx-imgui.h>
#pragma GCC diagnostic pop

#endif

using namespace vx;

/// Utils to go from RGB to HSB and vice versa

typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;

static hsv   rgb2hsv(rgb in);
static rgb   hsv2rgb(hsv in);

hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = static_cast<double>(NAN);                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}


rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = static_cast<long>(hh);
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}

/// Constructors

Color::Color() {
    this->_rf = 0.0f;
    this->_gf = 0.0f;
    this->_bf = 0.0f;
    this->_af = 1.0f;
    this->_r = 0;
    this->_g = 0;
    this->_b = 0;
    this->_a = 255;
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}

Color::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    this->_r = r;
    this->_g = g;
    this->_b = b;
    this->_a = a;
    this->_rf = static_cast<float>(r) / 255.0f;
    this->_gf = static_cast<float>(g) / 255.0f;
    this->_bf = static_cast<float>(b) / 255.0f;
    this->_af = static_cast<float>(a) / 255.0f;
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}

Color::Color(float r, float g, float b, float a) {
    this->_rf = r;
    this->_gf = g;
    this->_bf = b;
    this->_af = a;
    this->_r = static_cast<uint8_t>(r * 255.0f);
    this->_g = static_cast<uint8_t>(g * 255.0f);
    this->_b = static_cast<uint8_t>(b * 255.0f);
    this->_a = static_cast<uint8_t>(a * 255.0f);
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}

Color::Color(const Color &c) : Color(c.r(), c.g(), c.b(), c.a()) {}

const uint8_t& Color::r() const { return this->_r; }
const uint8_t& Color::g() const { return this->_g; }
const uint8_t& Color::b() const { return this->_b; }
const uint8_t& Color::a() const { return this->_a; }

const float& Color::rf() const { return this->_rf; }
const float& Color::gf() const { return this->_gf; }
const float& Color::bf() const { return this->_bf; }
const float& Color::af() const { return this->_af; }

const unsigned int& Color::u32() const { return this->_u32; }

void Color::set(float r, float g, float b, float a) {
    this->_rf = r;
    this->_gf = g;
    this->_bf = b;
    this->_af = a;

    this->applyInternals();
}

void Color::setAlpha(float a) {
    this->_af = a;

    this->applyInternals();
}

void Color::makeRoomForBrightnessDiff(double diffPercentage) {
    
    rgb c1 = rgb{
        static_cast<double>(this->_rf),
        static_cast<double>(this->_gf),
        static_cast<double>(this->_bf)
    };
    hsv c2 = rgb2hsv(c1);
    
    if (diffPercentage > 0.0) {
        if (c2.v > 1.0 - diffPercentage) c2.v = 1.0 - diffPercentage;
    } else {
        if (c2.v < -diffPercentage) c2.v = -diffPercentage;
    }
    
    c1 = hsv2rgb(c2);
    
    this->_rf = static_cast<float>(c1.r);
    this->_gf = static_cast<float>(c1.g);
    this->_bf = static_cast<float>(c1.b);
    
    this->_r = static_cast<uint8_t>(this->_rf * 255.0f);
    this->_g = static_cast<uint8_t>(this->_gf * 255.0f);
    this->_b = static_cast<uint8_t>(this->_bf * 255.0f);
    
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}

void Color::applyBrightnessDiff(double diffPercentage) {
    
    rgb c1 = rgb{
        static_cast<double>(this->_rf),
        static_cast<double>(this->_gf),
        static_cast<double>(this->_bf)
    };
    hsv c2 = rgb2hsv(c1);
    
    c2.v += diffPercentage;
    if (c2.v > 1.0) c2.v = 1.0;
    else if (c2.v < 0.0) c2.v = 0.0;

    c1 = hsv2rgb(c2);
    
    this->_rf = static_cast<float>(c1.r);
    this->_gf = static_cast<float>(c1.g);
    this->_bf = static_cast<float>(c1.b);
    
    this->_r = static_cast<uint8_t>(this->_rf * 255.0f);
    this->_g = static_cast<uint8_t>(this->_gf * 255.0f);
    this->_b = static_cast<uint8_t>(this->_bf * 255.0f);
    
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}

void Color::multScalar(float f) {
    this->set(this->_rf * f, this->_gf * f, this->_bf * f, this->_af * f);
}

void Color::multScalar2(const Color& c, float f) {
    this->set(c._rf * f, c._gf * f, c._bf * f, c._af * f);
}

void Color::multColor(const Color& c) {
    this->set(this->_rf * c._rf, this->_gf * c._gf, this->_bf * c._bf, this->_af * c._af);
}

void Color::multColor2(const Color& c1, const Color& c2) {
    this->set(c1._rf * c2._rf, c1._gf * c2._gf, c1._bf * c2._bf, c1._af * c2._af);
}

void Color::lerp(const Color& c1, const Color& c2, float percentage) {
    this->set(c1._rf * (1.0f - percentage) + c2._rf * percentage,
              c1._gf * (1.0f - percentage) + c2._gf * percentage,
              c1._bf * (1.0f - percentage) + c2._bf * percentage,
              c1._af * (1.0f - percentage) + c2._af * percentage);
}

void Color::applyInternals() {
    this->_r = static_cast<uint8_t>(this->_rf * 255.0f);
    this->_g = static_cast<uint8_t>(this->_gf * 255.0f);
    this->_b = static_cast<uint8_t>(this->_bf * 255.0f);
    this->_a = static_cast<uint8_t>(this->_af * 255.0f);
#ifndef P3S_CLIENT_HEADLESS
    this->_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(this->_rf, this->_gf, this->_bf, this->_af));
#else
    this->_u32 = 0;
#endif
}


