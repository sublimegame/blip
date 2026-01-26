//
//  VXEasing.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 5/29/20.
//

#include "VXEasing.hpp"

// C++
#include <cmath>

float _easeOutQuad(float f) {
    return 1.0f - pow(1.0f - f, 2.0f);
}

float _easeOutCubic(float f) {
    return 1.0f - pow(1.0f - f, 3.0f);
}

float _easeOutExpo(float f) {
    return f == 1.0f ? 1.0f : 1.0f - pow(2.0f, -10.0f * f);
}

float vx::ease(float f, Easing e) {
    switch (e) {
        case vx::Easing::easeOutQuad:
            return _easeOutQuad(f);
            
        case vx::Easing::easeOutCubic:
            return _easeOutCubic(f);
            
        case vx::Easing::easeOutExpo:
            return _easeOutExpo(f);
    }
    return f;
}



