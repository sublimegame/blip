//
//  VXMenu.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 21/02/2020.
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

// C/C++
#include <list>

// vx
#include "VXNode.hpp"
#include "VXNotifications.hpp"
#include "VXTickDispatcher.hpp"
#include "VXKeyboard.hpp"
#include "VXVirtualKeyboardToolbar.hpp"
#include "VXPopup.hpp"

// engine
#include "inputs.h"

namespace vx {

///
/// Menu represent a state of the app's "menu" layer
/// This is not expected to be instanciated but only inherited.
///
class Menu;
typedef std::shared_ptr<Menu> Menu_SharedPtr;
typedef std::weak_ptr<Menu> Menu_WeakPtr;

class Menu final : public vx::TickListener, public NotificationListener {
    
public:
    
    ///
    static Menu_SharedPtr make();
    
    ///
    virtual ~Menu() override;
    
    // NOTE: we could get rid of this and use specific functions
    // when there's something to do with the root node.
    const Node_SharedPtr& getRootNode() const;
    
    ///
    const Node_SharedPtr& getContainerNode() const;

    ///
    void processInputs(const double dt);
    
    ///
    void tick(const double dt) override;
    
    ///
    void processSpecialChar(CharEvent);
    
    void didReceiveNotification(const vx::Notification &notification) override;
    
    /// If true, it's ok to hand over input state and events to imgui
    KeyboardType activeKeyboard();
    
    ///
    void showPopup(const Popup_SharedPtr& popup);
    
private:
    
    /// Constructor
    Menu();
    
    ///
    void _init(const Menu_SharedPtr& ref);
    
    //
    void _reset();
    
    /// Weak reference on self to make it possible to call functions
    /// on Node pointers while manipuling shared pointers.
    /// Should not be defined in derived classes,
    /// however it should be set on the factory methods of derived classes
    Menu_WeakPtr _weakSelf;
    
    ///
    Node_SharedPtr _rootNode;
    
    /// Contains the _rootNode and other possible nodes
    /// like the virtual keyboard toolbar.
    /// It would be nice to host a popup layer here too.
    Node_SharedPtr _containerNode;
    
    ///
    VirtualKeyboardToolbar_SharedPtr _virtualKeyboardToolbar;
    
    ///
    InputListener *_inputListener;
    
    /// Improves navigation by having persistent keyboard until a final keyboard type is received
    KeyboardType _kbNavigation;
    
    /// Popup that's currently displayed
    Popup_SharedPtr _activePopup;
    
    //
    std::map<uint8_t, bool> _touchMayUnfocusOnRelease;
    bool _leftClickMayUnfocusOnRelease;
};

}

#pragma clang diagnostic pop
