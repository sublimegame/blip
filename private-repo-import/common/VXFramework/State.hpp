//
//  State.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 21/02/2020.
//

#pragma once

// C++
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>

namespace vx {

class State;
typedef std::shared_ptr<State> State_SharedPtr;
typedef std::weak_ptr<State> State_WeakPtr;

class State final
{
public:
    
    typedef std::function<void(void *userdata)> UserDataFreeFn;

    enum Value : uint8_t {
        startupLoading, // first screen displayed when launching the app
        noActiveGame, // 
        gameRunning, // running game
        // gamePaused,
        activeWorldCodeEditor,
        activeWorldCodeReader,
        worldCodeEditor,
    };
    
    static State_SharedPtr make(const Value& state);
    static State_SharedPtr make(const Value& state, void *userData, UserDataFreeFn userDataFreeFn);
    
    State(const State&) = delete; // remove copy constructor
    State(State&&) = delete;
    State& operator=(const State&) = delete;
    State& operator=(State&&) = delete;
    
    ~State();
    
    /// Returns true if State's Value is equal to given Value
    bool is(const Value& state);
    
    /// Returns true if State's Value is not equal to given Value
    bool isNot(const Value& state);
    
    /// Returns true if it's ok to move to given State Value.
    bool canTransitionTo(const Value& state);
    
    // Allows switch and comparisons
    Value getValue() const { return _value; }
    
    /// Returns string state name, useful for debug
    std::string toString() const;
    
private:
    
    static std::map<Value, std::vector<Value>> _allowedTransitions;
    
    State(const Value& state);
    State(const Value& state, void *userData, UserDataFreeFn userDataFreeFn);
    
    Value _value;
    void *_userData;
    UserDataFreeFn _userDataFreeFn;
};

} // namespace vx

