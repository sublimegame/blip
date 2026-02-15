//
//  VXTickDispatcher.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/24/20.
//

#include "VXTickDispatcher.hpp"

// C++
#include <algorithm>

using namespace vx;

// ---------------------
// TickListener
// ---------------------

TickListener::TickListener() {
    TickDispatcher::getInstance().addListener(this);
}

TickListener::~TickListener() {
    TickDispatcher::getInstance().removeListener(this);
}

void TickListener::tick(const double dt) {
    // doesn't do anything by default
    // meant to be overriden
}

// ---------------------
// TickDispatcher
// ---------------------

TickDispatcher::TickDispatcher() {
    this->_listeners.clear();
}

TickDispatcher::~TickDispatcher() {}

void TickDispatcher::tick(const double dt) {
    for (TickListener *l : this->_listeners) {
        l->tick(dt);
    }
}
    
void TickDispatcher::addListener(TickListener *l) {
    if (std::find(this->_listeners.begin(), this->_listeners.end(), l) != this->_listeners.end()) {
        // listener found, should not be duplicated
        return;
    }
    this->_listeners.push_back(l);
}

void TickDispatcher::removeListener(TickListener *l) {
    this->_listeners.remove(l);
}
