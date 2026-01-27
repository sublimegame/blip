//
//  VXColor.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/19/20.
//

#pragma once

// C/C++
#include "stdint.h"

namespace vx {

class Color {
    
public:
    
    /// Constructor
    Color();
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    Color(float r, float g, float b, float a);
    Color(const Color &c);
    
    const uint8_t &r() const;
    const uint8_t &g() const;
    const uint8_t &b() const;
    const uint8_t &a() const;
    
    const float &rf() const;
    const float &gf() const;
    const float &bf() const;
    const float &af() const;
    
    const unsigned int &u32() const;

    /// Setters, refresh internal values
    /// Notes: for fade in/out animation, use setAlpha only, since alpha is based of 100 and RGB based of 255,
    /// animation periods won't be in sync
    void set(float r, float g, float b, float a);
    void setAlpha(float a);

    /// HSV value operations
    void applyBrightnessDiff(double diffPercentage);
    
    /// makes sure there's enough room for brightness diff
    void makeRoomForBrightnessDiff(double diffPercentage);
    
    /// Linear RGB operations, enough for most situations
    void multScalar(float f);
    void multScalar2(const Color& c, float f);
    void multColor(const Color& c);
    void multColor2(const Color& c1, const Color& c2);
    
    void lerp(const Color& c1, const Color& c2, float percentage);
    
private:
    /** 0 to 255 */
    uint8_t _r;
    uint8_t _g;
    uint8_t _b;
    uint8_t _a;
    
    /** 0.0f to 1.0f */
    float _rf;
    float _gf;
    float _bf;
    float _af;
    
    unsigned int _u32;

    void applyInternals();
};

}
