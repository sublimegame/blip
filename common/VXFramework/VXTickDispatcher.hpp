//
//  VXTickDispatcher.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/24/20.
//

#pragma once

// C++
#include <string>
#include <list>

// xptools
#include "Macros.h"

namespace vx {

class TickListener {

public:
    TickListener();
    virtual ~TickListener();

    /// Called when receiving tick
    virtual void tick(const double dt);
};

class TickDispatcher final {

public:
    static TickDispatcher& getInstance() {
        static TickDispatcher instance;
        return instance;
    }

    /**
     * Notifies all listeners
     * Tick will be received in same thread where `tick` is called.
     */
    void tick(const double dt);
    
    void addListener(TickListener *l);

    void removeListener(TickListener *l);

    VX_DISALLOW_COPY_AND_ASSIGN(TickDispatcher)

private:
    TickDispatcher();
    ~TickDispatcher();

    std::list<TickListener*> _listeners;
};

}
