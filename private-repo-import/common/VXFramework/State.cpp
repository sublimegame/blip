//
//  State.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 02/03/2020.
//

#include "State.hpp"

#include "config.h"

using namespace vx;

std::map<State::Value, std::vector<State::Value>> State::_allowedTransitions = {
    {State::startupLoading, {
        State::gameRunning,
        State::noActiveGame,
    }},
    {State::noActiveGame, {
        State::gameRunning,
        State::noActiveGame,
    }},
    {State::activeWorldCodeEditor, {
        State::gameRunning,
        State::startupLoading, /*disconnected, coming from direct link*/
        State::noActiveGame
    }},
    {State::activeWorldCodeReader, {
        State::gameRunning,
        State::startupLoading, /*disconnected, coming from direct link*/
        State::noActiveGame
    }},
    {State::worldCodeEditor, {
        State::gameRunning,
        State::startupLoading, /*disconnected, coming from direct link*/
        State::noActiveGame
    }},
    {State::gameRunning, {
        State::activeWorldCodeReader,
        State::activeWorldCodeEditor,
        State::worldCodeEditor,
        State::gameRunning,
        State::startupLoading, /*disconnected, coming from direct link*/
        State::noActiveGame
    }},
};

State_SharedPtr State::make(const Value& state) {
    State_SharedPtr s(new State(state));
    return s;
}

State_SharedPtr State::make(const Value& state, void *userData, UserDataFreeFn userDataFreeFn) {
    State_SharedPtr s(new State(state, userData, userDataFreeFn));
    return s;
}

State::State(const Value& state) :
_value(state),
_userData(nullptr),
_userDataFreeFn(nullptr) {}

State::State(const Value& state, void *userData, UserDataFreeFn userDataFreeFn) :
_value(state),
_userData(userData),
_userDataFreeFn(userDataFreeFn) {}

State::~State() {
    if (_userData != nullptr && _userDataFreeFn != nullptr) {
        _userDataFreeFn(_userData);
        _userData = nullptr;
        _userDataFreeFn = nullptr;
    }
}

bool State::is(const Value& state) {
    return this->_value == state;
}

bool State::isNot(const Value& state) {
    return this->_value != state;
}

bool State::canTransitionTo(const Value& state) {
    try {
        std::vector<State::Value> allowed = State::_allowedTransitions.at(this->_value);
        for (State::Value v : allowed) {
            if (v == state) { return true; }
        }
    }
    catch(...) {}
    return false;
}

std::string State::toString() const {
    switch (_value) {
        case State::startupLoading:
            return "startupLoading";
        case State::noActiveGame:
            return "noActiveGame";
        case State::gameRunning:
            return "gameRunning";
        case State::activeWorldCodeEditor:
            return "activeWorldCodeEditor";
        case State::activeWorldCodeReader:
            return "activeWorldCodeReader";
        case State::worldCodeEditor:
            return "worldCodeEditor";
    }
    // this should not happen
    vx_assert("State value not supported");
    return "";
}
