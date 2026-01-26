//
//  VXKeyboard.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 14/05/2020.
//

#pragma once

namespace vx {

///
enum KeyboardType {
    KeyboardTypeNone = 0,           // no keyboard (hidden)
    KeyboardTypeReturnOnReturn = 1, // "return"
    KeyboardTypeNextOnReturn = 2,   // "next"
    KeyboardTypeDoneOnReturn = 3,   // "done"
};

}
