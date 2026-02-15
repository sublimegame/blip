//
//  VXMenu.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 21/02/2020.
//

#include "VXMenu.hpp"

// C++
#include <cmath>

#include "sbs.hpp"
#include "scripting.hpp"
#include "GameCoordinator.hpp"
#include "VXApplication.hpp"

// xptools
#include "xptools.h"
#include "vxlog.h"

using namespace vx;

///
Menu_SharedPtr Menu::make() {
    Menu_SharedPtr p(new Menu);
    p->_init(p);
    return p;
}

///
void Menu::_init(const Menu_SharedPtr& ref) {
    _weakSelf = ref;
    
    _reset();
    
    _containerNode = Node::make();
    _containerNode->setColor(MenuConfig::shared().colorTransparent);
    _containerNode->setMaxTouches(3);

    _containerNode->parentDidResize = [](Node_SharedPtr n){
        n->setSize(Screen::widthInPoints, Screen::heightInPoints);
        n->setPos(0.0f, Screen::heightInPoints);
    };

    _containerNode->parentDidResizeWrapper();
}

void Menu::_reset() {
    for (uint8_t i = 0; i < 10; ++i) {
        _touchMayUnfocusOnRelease[i] = false;
    }
    _leftClickMayUnfocusOnRelease = false;
    
    Node::Manager::shared()->unsetAllHoveredAndPressed();
    Node::Manager::shared()->loseFocus();
}

Menu::Menu() : TickListener(),
_weakSelf(),
_rootNode(nullptr),
_containerNode(nullptr),
_virtualKeyboardToolbar(nullptr) {

    _inputListener = input_listener_new(true, true, // mouse events, touch events
                                        true, false, // key events, char events
                                        false, // dir pad events
                                        false, // action pad events
                                        false, // camera analog events
                                        false, false // repeated key down, repeated char
                                        );
    _kbNavigation = KeyboardTypeNone;
    
    NotificationCenter::shared().addListener(this, NotificationName::didResize);
    NotificationCenter::shared().addListener(this, NotificationName::showKeyboard);
    NotificationCenter::shared().addListener(this, NotificationName::hideKeyboard);
}

Menu::~Menu() {
    input_listener_free(_inputListener);
    NotificationCenter::shared().removeListener(this);
}

const Node_SharedPtr& Menu::getRootNode() const {
    return _rootNode;
}

const Node_SharedPtr& Menu::getContainerNode() const {
    return _containerNode;
}

void Menu::tick(const double dt) {

    const Node_SharedPtr& newRootNode = GameCoordinator::getInstance()->getCurrentRootNode();
    
    // Not ideal to check that in tick, but needed for popup to be released
    // and Lua sandbox to be informed that C++ menu is no longer active
    if (_activePopup != nullptr && _activePopup->getParent() == nullptr) {
        _activePopup = nullptr;
    }
            
    if (_rootNode != newRootNode) {
        _reset();
        
        if (_rootNode != nullptr) {
            _rootNode->removeFromParent();
        }
        
        _rootNode = newRootNode;
        _rootNode->parentDidResize = [](Node_SharedPtr n){
            Node_SharedPtr parent = n->getParent();
            if (parent == nullptr) { return; }
            n->setSize(parent->getWidth(), parent->getHeight());
            n->setPos(0.0f, parent->getHeight());
        };
        _containerNode->addChild(_rootNode);
        
        if (_activePopup != nullptr) {
            // remove and re-add to make sure it's displayed on top
            _activePopup->removeFromParent();
            _containerNode->addChild(_activePopup);
        }
    }
    
    if (_rootNode != nullptr && _rootNode->getChildrenWantAutofocus() == true) {
        // A node in the hierarchy, wants to be in focus.
        // Retrieve that node.
        Node_SharedPtr nodeWantingAutofocus = _rootNode->getChildWantingAutofocusInHierarchy().lock();
        if (nodeWantingAutofocus != nullptr) {
            if (nodeWantingAutofocus != nullptr) {
                Node::Manager::shared()->setFocused(nodeWantingAutofocus);
            }
        }
    }
    
    // keyboard persistence to improve navigation
    Node_SharedPtr focused = Node::Manager::shared()->focused().lock();
    KeyboardType newKbType = (focused != nullptr) ? focused->getExpectedKeyboard() : KeyboardTypeNone;
    if (_kbNavigation != newKbType) {
        _kbNavigation = newKbType;
    }
}

void Menu::didReceiveNotification(const vx::Notification &notification) {
    switch (notification.name) {
        case NotificationName::didResize:
        {
            if (this->_containerNode != nullptr) {
                this->_containerNode->parentDidResizeWrapper();
            }
            break;
        }
        case NotificationName::showKeyboard:
        {
            // keyboardHeight == 0.0 here means hardware keyboard in use
            // in that case, do not display the virtual keyboard toolbar
            float keyboardHeight = GameCoordinator::getInstance()->keyboardSize().getHeight();
            
            bool cppInputFocused = false;
            
            Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
            Node_SharedPtr focused = focusedWeak.lock();
            if (focused != nullptr) {
                CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
                if (codeEditor != nullptr) {
                    cppInputFocused = true;
                }
            }
            
            if (_rootNode != nullptr) {
                if (cppInputFocused == false) {
                    
                    if (_virtualKeyboardToolbar != nullptr) {
                        _virtualKeyboardToolbar->removeFromParent();
                        _virtualKeyboardToolbar = nullptr;
                    }
                    _rootNode->parentDidResize = [](Node_SharedPtr n){
                        Node_SharedPtr parent = n->getParent();
                        if (parent == nullptr) { return; }
                        n->setSize(parent->getWidth(), parent->getHeight());
                        n->setPos(0.0f, parent->getHeight());
                    };
                    _rootNode->parentDidResizeWrapper();
                    
                } else { // keyboard shown for cpp input
                    if (keyboardHeight > 0.0f) {
                        // Showing keyboard for C++ menu: need to reduce _rootNode size + show C++ toolbar
                        if (activeKeyboard() != KeyboardTypeNone && _virtualKeyboardToolbar == nullptr) {
                            _virtualKeyboardToolbar = VirtualKeyboardToolbar::make();
                            _containerNode->addChild(_virtualKeyboardToolbar);
                        }

                        if (activeKeyboard() != KeyboardTypeNone) {
                            VirtualKeyboardToolbar_WeakPtr toolbarWeak = VirtualKeyboardToolbar_WeakPtr(_virtualKeyboardToolbar);

                            _rootNode->parentDidResize = [toolbarWeak, keyboardHeight](Node_SharedPtr n){
                                Node_SharedPtr parent = n->getParent();
                                if (parent == nullptr) { return; }

                                VirtualKeyboardToolbar_SharedPtr toolbar = toolbarWeak.lock();
                                if (toolbar == nullptr) { return; }

                                toolbar->setWidth(parent->getWidth());
                                toolbar->setPos(0.0f, keyboardHeight + toolbar->getHeight());

                                n->setSize(parent->getWidth(), parent->getHeight() - toolbar->getTop());
                                n->setPos(0.0f, parent->getHeight());
                            };
                            _rootNode->parentDidResizeWrapper();
                        }
                    } else { // hardware keyboard
                        if (_virtualKeyboardToolbar != nullptr) {
                            _virtualKeyboardToolbar->removeFromParent();
                            _virtualKeyboardToolbar = nullptr;
                            
                            _rootNode->parentDidResize = [](Node_SharedPtr n){
                                Node_SharedPtr parent = n->getParent();
                                if (parent == nullptr) { return; }
                                n->setSize(parent->getWidth(), parent->getHeight());
                                n->setPos(0.0f, parent->getHeight());
                            };
                            _rootNode->parentDidResizeWrapper();
                        }
                    }
                }
            }
            
            if (activeKeyboard() != KeyboardTypeNone) {
                // Showing keyboard for C++ menu, reducing _containerNode size
            }
            break;
        }
        case NotificationName::hideKeyboard:
        {
            if (_virtualKeyboardToolbar != nullptr) {
                _virtualKeyboardToolbar->removeFromParent();
                _virtualKeyboardToolbar = nullptr;
            }

            if (_rootNode != nullptr) {
                _rootNode->parentDidResize = [](Node_SharedPtr n){
                    Node_SharedPtr parent = n->getParent();
                    if (parent == nullptr) { return; }
                    n->setSize(parent->getWidth(), parent->getHeight());
                    n->setPos(0.0f, parent->getHeight());
                };
                _rootNode->parentDidResizeWrapper();
            }

            Node::Manager::shared()->loseFocus();
            break;
        }
        default:
            break;
    }
}

void Menu::processInputs(const double dt) {
    
    if (_rootNode == nullptr) { return; }
    if (_containerNode == nullptr) { return; }
    
    // initial pointer down position used to check for drag threshold or long press
    static float firstDownX = -1.0f;
    static float firstDownY = -1.0f;
    static bool isDragging = false;
    // timer used to check when to start a long press, if pointer does not move
    static float longPressTimer = 0.0f;
    static bool isLongPressDragging = false;
    
    float localX = 0.0f;
    float localY = 0.0f;
    Node_SharedPtr scrollingNode(nullptr);
    
    // long press timer initiated
    if (longPressTimer > 0.0f) {
        longPressTimer += static_cast<float>(dt);
        if (longPressTimer >= VXMENU_LONGPRESS_THRESHOLD) {
            if (Node::Manager::shared()->setLongPressed()) {
                isLongPressDragging = true;
            }
            longPressTimer = 0.0f;
        } else {
            Node::Manager::shared()->setLongPressPercentage(longPressTimer / VXMENU_LONGPRESS_THRESHOLD, firstDownX, firstDownY);
        }
    }

    // ------------
    // mouse events
    // ------------
    
    const MouseEvent *me = input_listener_pop_mouse_event(this->_inputListener);
    while (me != nullptr) {
        Node_SharedPtr n = this->_containerNode->hitTest(me->x, me->y, &localX, &localY, scrollingNode);
        
        if (me->move) {
            const float dx = me->x - firstDownX;
            const float dy = me->y - firstDownY;
            const bool dragThreshold = dx * dx + dy * dy >= VXMENU_DRAG_SQR_THRESHOLD;
    
            // cancel the long press attempt if a drag may be triggered
            if (dragThreshold) {
                longPressTimer = 0.0f;
            }
            
            // update hovered node
            if (n != nullptr) {
                if (Node::Manager::shared()->hovered(0) != n) {
                    // hovering a different node
                    if (Node::Manager::shared()->hovered(0) != nullptr) {
                        Node::Manager::shared()->hovered(0)->leave();
                    }
                    Node::Manager::shared()->setHovered(0, n);
                }
                Node::Manager::shared()->hovered(0)->hover(localX, localY, me->dx, me->dy);
            } else if (Node::Manager::shared()->hovered(0) != nullptr) {
                Node::Manager::shared()->hovered(0)->leave();
                Node::Manager::shared()->setHovered(0, nullptr);
            }
            
            // check/update dragging
            int moveIndex = -1; // invalid
            if (Node::Manager::shared()->pressed(INDEX_MOUSE_LEFTBUTTON) != nullptr) {
                moveIndex = INDEX_MOUSE_LEFTBUTTON;
            } else if (Node::Manager::shared()->pressed(INDEX_MOUSE_RIGHTBUTTON) != nullptr) {
                moveIndex = INDEX_MOUSE_RIGHTBUTTON;
            }
            if (moveIndex != -1 && (dragThreshold || isDragging)) {
                Node::Manager::shared()->pressed(moveIndex)->localPoint(me->x, me->y, localX, localY);
                Node::Manager::shared()->move(moveIndex, localX, localY, me->dx, me->dy);
                isDragging = true;
            }
            
        } else if (me->button == MouseButtonLeft) {
            if (me->down) {
                
                _leftClickMayUnfocusOnRelease = false;
                Node_SharedPtr focused = Node::Manager::shared()->focused().lock();
    
                firstDownX = me->x;
                firstDownY = me->y;
                
                if (n != nullptr) {
                    if (n->press(INDEX_MOUSE_LEFTBUTTON, localX, localY)) {
                        Node::Manager::shared()->setPressed(INDEX_MOUSE_LEFTBUTTON, n);
                        longPressTimer = static_cast<float>(dt);
                    }
                    
                    if (focused != nullptr && n != focused && n->getUnfocusesFocusedNode()) {
                        _leftClickMayUnfocusOnRelease = true;
                    }
                } else {
                    // by default, when not hitting a node, left click may onfocus on release.
                    if (focused != nullptr) {
                        _leftClickMayUnfocusOnRelease = true;
                    }
                }
            } else { // up
                Node_SharedPtr focused = Node::Manager::shared()->focused().lock();
                if (focused != nullptr) {
                    if (_leftClickMayUnfocusOnRelease &&
                        (n == nullptr || n != focused)) { // released outside of focused node
                        
                        Node::Manager::shared()->loseFocus();
                    }
                }
                
                if (Node::Manager::shared()->pressed(INDEX_MOUSE_LEFTBUTTON) != nullptr) {
                    // see if release action happens on the Node::pressed
                    if (n == Node::Manager::shared()->pressed(INDEX_MOUSE_LEFTBUTTON)) {
                        Node::Manager::shared()->setFocused(n);
                        n->release(INDEX_MOUSE_LEFTBUTTON, localX, localY);
                        
                    } else {
                        Node::Manager::shared()->pressed(INDEX_MOUSE_LEFTBUTTON)->cancel(INDEX_MOUSE_LEFTBUTTON);
                    }
                    Node::Manager::shared()->setPressed(INDEX_MOUSE_LEFTBUTTON, nullptr);
                }
    
                isDragging = false;
                isLongPressDragging = false;
            }
            
        } else if (me->button == MouseButtonRight) {
            if (me->down) {
                if (n != nullptr) {
                    if (n->press(INDEX_MOUSE_RIGHTBUTTON, localX, localY)) {
                        Node::Manager::shared()->setPressed(INDEX_MOUSE_RIGHTBUTTON, n);
                    }
                }
            } else { // up
                if (Node::Manager::shared()->pressed(INDEX_MOUSE_RIGHTBUTTON) != nullptr) {
                    // see if release action happens on the Node::pressed
                    if (n == Node::Manager::shared()->pressed(INDEX_MOUSE_RIGHTBUTTON)) {
                        Node::Manager::shared()->setFocused(n);
                        n->release(INDEX_MOUSE_RIGHTBUTTON, localX, localY);
                    } else {
                        Node::Manager::shared()->pressed(INDEX_MOUSE_RIGHTBUTTON)->cancel(INDEX_MOUSE_RIGHTBUTTON);
                    }
                    Node::Manager::shared()->setPressed(INDEX_MOUSE_RIGHTBUTTON, nullptr);
                }
            }
            
        } else if (me->button == MouseButtonScroll) {
            if (scrollingNode != nullptr) {
                scrollingNode->scroll(-me->dx * VXMENU_SCROLL_MOUSE_FACTOR, -me->dy * VXMENU_SCROLL_MOUSE_FACTOR);
            } else if (n != nullptr) {
                if (n->getScrollHandling() == vx::Node::ScrollHandling::mouseWheelOnly) {
                    n->scroll(-me->dx * VXMENU_SCROLL_MOUSE_FACTOR, -me->dy * VXMENU_SCROLL_MOUSE_FACTOR);
                }
            }
        }
        
        me = input_listener_pop_mouse_event(this->_inputListener);
    }
    
    
    // ------------
    // touch events
    // ------------
    
    const TouchEvent *te = input_listener_pop_touch_event(this->_inputListener);
    
    // retrieve maxTouches value from root node
    const uint8_t maxTouches = _containerNode->getMaxTouches();
    
    while (te != nullptr) {
        
        if (te->ID >= Node::Manager::shared()->getMaxTouches() ||
            te->ID >= maxTouches) {
            te = input_listener_pop_touch_event(this->_inputListener);
            continue;
        }
        
        Node_SharedPtr n = this->_containerNode->hitTest(te->x, te->y, &localX, &localY, scrollingNode);
        
        if (te->move) {
            
            const float dx = te->x - firstDownX;
            const float dy = te->y - firstDownY;
            const bool dragThreshold = dx * dx + dy * dy >= VXMENU_DRAG_SQR_THRESHOLD;

            // cancel the long press attempt if a drag may be triggered
            if (dragThreshold) {
                longPressTimer = 0.0f;
            }
            
            Node_SharedPtr pressed = Node::Manager::shared()->pressed(te->ID);
            
            if (scrollingNode != nullptr && scrollingNode == pressed && isLongPressDragging == false) {
                // 1) are we currently dragging on a scrolling node?
                
                Node_SharedPtr pressed = Node::Manager::shared()->pressed(te->ID);
                if (isDragging) {
                    Node::Manager::shared()->addDragScrollDelta(scrollingNode, te->dy, te->ID);
                }
                // 2) should we initiate dragging on a scrolling node?
                else {
                    Node::Manager::shared()->startDragScroll(scrollingNode, te->ID);
                    isDragging = true;
                }
            }
            // 3) should we initiate/update dragging?
            else {
                Node_SharedPtr pressed = Node::Manager::shared()->pressed(te->ID);
                if (pressed != nullptr) {
                    pressed->localPoint(te->x, te->y, localX, localY);
                    Node::Manager::shared()->move(te->ID, localX, localY, te->dx, te->dy);
                }
            }
            
        } else if (te->state == TouchStateDown) { // down

            vx_assert(te->ID < TOUCH_EVENT_MAXCOUNT);
            
            _touchMayUnfocusOnRelease[te->ID] = false; // false by default
            Node_SharedPtr focused = Node::Manager::shared()->focused().lock();

            firstDownX = te->x;
            firstDownY = te->y;
            
            // did we touch a node?
            if (n != nullptr) {
                if (n->press(te->ID, localX, localY) == true) {
                    Node::Manager::shared()->setPressed(te->ID, n);
                    // initiate long press timer
                    longPressTimer = static_cast<float>(dt);
                }
                
                if (focused != nullptr && n != focused && n->getUnfocusesFocusedNode()) {
                    // indicate that this touch may unfocus focused node on release
                    _touchMayUnfocusOnRelease[te->ID] = true;
                }
            } else {
                // by default, when not hitting a node, touch may onfocus on release.
                if (focused != nullptr) {
                    _touchMayUnfocusOnRelease[te->ID] = true;
                }
            }
            
        } else if (te->state == TouchStateUp) { // up
            
            // clear scrolling
            if (scrollingNode != nullptr && isDragging) {
                Node::Manager::shared()->releaseDragScroll(te->ID);
            }
            isDragging = false;
            scrollingNode = nullptr;
            
            // clear focus
            Node_SharedPtr focused = Node::Manager::shared()->focused().lock();
            if (focused != nullptr) {
                if (_touchMayUnfocusOnRelease[te->ID] &&
                    (n == nullptr || n != focused)) { // released outside of focused node
                    
                    Node::Manager::shared()->loseFocus();
                }
                // otherwise, keep focus
            }
            
            // clear press
            if (Node::Manager::shared()->pressed(te->ID) != nullptr) {
                // see if release action happens on the Node::pressed
                if (n == Node::Manager::shared()->pressed(te->ID)) {
                    Node::Manager::shared()->setFocused(n);
                    n->release(te->ID, localX, localY);
                } else {
                    Node::Manager::shared()->pressed(te->ID)->cancel(te->ID);
                }
                Node::Manager::shared()->setPressed(te->ID, nullptr);
            }
            
            // clear long press
            longPressTimer = 0.0f;
            isLongPressDragging = false;
            
        } else { // other states
            if (Node::Manager::shared()->pressed(te->ID) != nullptr) {
                Node::Manager::shared()->pressed(te->ID)->cancel(te->ID);
                Node::Manager::shared()->setPressed(te->ID, nullptr);
            }
            
            Node::Manager::shared()->stopDragScroll();
        }
        
        te = input_listener_pop_touch_event(this->_inputListener);
    }
    
    // ------------
    // key events
    // ------------
    
    const KeyEvent *ke = input_listener_pop_key_event(this->_inputListener);
    while (ke != nullptr) {
        if (ke->state == KeyStateUp) {
            // azerty-sensitive keycodes are processed as charcodes instead, see processSpecialChar
            switch (ke->input) {
                case InputEsc:
                    NotificationCenter::shared().notify(NotificationName::escKeyUp);
                    break;
                case InputReturn:
                    NotificationCenter::shared().notify(NotificationName::returnKeyUp);
                    break;
                case InputReturnKP:
                    NotificationCenter::shared().notify(NotificationName::returnKeyUp);
                    break;
                default:
                    break;
            }
        }
        ke = input_listener_pop_key_event(this->_inputListener);
    }
}

void Menu::processSpecialChar(CharEvent ce) {
    switch(ce.inputChar) {
        case 47: // unicode for slash
            NotificationCenter::shared().notify(NotificationName::slashKeyUp);
            break;
        default:
            break;
    }
}

KeyboardType Menu::activeKeyboard() {
    return _kbNavigation;
}

void Menu::showPopup(const Popup_SharedPtr& popup) {
    if (_activePopup != nullptr) {
        _activePopup->removeFromParent();
    }
    _activePopup = popup;
    _containerNode->addChild(_activePopup);
}
