//
//  VXEasing.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 5/29/20.
//

#pragma once

namespace vx {

enum class Easing {
    easeOutQuad,
    easeOutCubic,
    easeOutExpo
};

float ease(float f, Easing e);

} // vx
