//
//  VXValueAnimator.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 5/29/20.
//

#include "VXValueAnimator.hpp"

// xptools
#include "xptools.h"

using namespace vx;

FloatAnimator::FloatAnimator(float start, float end, double duration, Easing e) {
    this->_start = start;
    this->_end = end;
    this->_diff = this->_end - this->_start;
    this->_duration = duration;
    this->_current = 0.0;
    this->_easing = e;
}

void FloatAnimator::reset() {
    this->_current = 0.0;
}

void FloatAnimator::reset(float start) {
    this->_current = 0.0;
    this->_start = start;
    this->_diff = this->_end - this->_start;
}

float FloatAnimator::tick(const double dt) {
    
    this->_current += dt;
    
    // don't go past duration
    if (this->_current > this->_duration) {
        this->_current = this->_duration;
    }
    
    float progression = static_cast<float>(this->_current / this->_duration);
    
    progression = ease(progression, this->_easing);

    return this->_start + this->_diff * progression;
}
