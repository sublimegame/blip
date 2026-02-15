//
//  VXJSON.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 04/08/2022.
//

#pragma once

#include "xptools.h"

namespace vx {

// Utils on top of cJSON
class JSON final {
public:
    // returns true if found
    static bool readString(const cJSON* node, const char* field, std::string& outValue);
    
    // returns true if found
    static bool readInt(const cJSON* node, const char* field, int& outValue);
    
    // returns true if found
    static bool readDouble(const cJSON* node, const char* field, double& outValue);
    
    // returns true if found
    static bool readBool(const cJSON* node, const char* field, bool& outValue);
};

}
