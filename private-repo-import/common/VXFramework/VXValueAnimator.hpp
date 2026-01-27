//
//  VXValueAnimator.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 5/29/20.
//

#pragma once

#include "VXEasing.hpp"

namespace vx {

class FloatAnimator {
  
public:
    
    /// Constructor
    FloatAnimator(float start, float end, double duration, Easing e);
    
    /// Resets animation with all original values
    void reset();
    
    /// Resets animation with new start value
    void reset(float start);
    
    /// Applies tick and returns updated float value
    float tick(const double dt);
    
private:
    
    float _start;
    float _end;
    float _diff;
    double _duration;
    double _current; // % progression = _current / _duration
    Easing _easing;
    
};

}
