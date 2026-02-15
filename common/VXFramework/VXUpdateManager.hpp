//
//  VXUpdateManager.hpp
//  Cubzh
//
//  Created by Xavier Legland on 2/14/22.
//

#pragma once

// C++
#include <string>

namespace vx {

class UpdateManager final {
    
public:
    
    ///
    static UpdateManager& shared() {
        static UpdateManager m;
        return m;
    }
    
    bool firstVersionRun() const;
    void stampCurrentVersion();
    
private:
    
    UpdateManager();
    ~UpdateManager();

    std::string _version;
    int _buildNumber;
};

}
