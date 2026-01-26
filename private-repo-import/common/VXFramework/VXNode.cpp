//
//  VXNode.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 10/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#include "VXNode.hpp"

// C++
#include <cmath>
#include <algorithm> // for std::find

// vx
#include "VXConfig.hpp"

// xptools
#include "xptools.h"

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// bgfx+imgui
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#pragma GCC diagnostic pop

#endif

using namespace vx;

// ------------------------------
// Node::Manager
// ------------------------------

Node::Manager *Node::Manager::_instance = nullptr;

Node::Manager *Node::Manager::shared() {
    if (Node::Manager::_instance == nullptr) {
        Node::Manager::_instance = new Manager();

    }
    return Node::Manager::_instance;
}

Node::Manager::Manager() : vx::TickListener(),
_maxTouches(TOUCH_AND_MOUSE_EVENT_MAXINDEXCOUNT),
_pressed(_maxTouches),
_hovered(_maxTouches),
_dragScrolled(),
_dragScrollTouchID(0),
_dragScrolledReleased(false),
_dragScrollDeltaY(0.0f),
_dragScrollLastDeltaY(0.0f),
_dragScrollDidStart(false),
_focused(),
_longPressDT(0),
_longPressDX(0.0f),
_longPressDY(0.0f) {
    this->_scrollYVelocityAnimator = new FloatAnimator(0.0f, 0.0f, 1.0, Easing::easeOutCubic);
    this->unsetAllHoveredAndPressed();
    NotificationCenter::shared().addListener(this, NotificationName::escKeyUp);
    NotificationCenter::shared().addListener(this, NotificationName::softReturnKeyUp);
    NotificationCenter::shared().addListener(this, NotificationName::returnKeyUp);
}

Node::Manager::~Manager() {}

uint8_t Node::Manager::getMaxTouches() {
    return this->_maxTouches;
}

Node_SharedPtr Node::Manager::pressed(const uint8_t index) {
    return this->_pressed[index].lock();
}

void Node::Manager::setPressed(uint8_t index, const Node_SharedPtr& node) {
    if (index >= this->_maxTouches) return;

    this->_pressed[index] = node;

    this->_longPressDT = -1.0;

    if (node != nullptr) {
        int n = 0;
        for (uint8_t i = 0; i < this->_maxTouches; i++) {
            if (this->_pressed[i].expired() == false) {
                n += 1;
                if (n > 1) { break; }
            }
        }
        if (n == 1) {
            _longPressDT = 0.0;
            _longPressDX = 0.0f;
            _longPressDY = 0.0f;
        }
    }
}

bool Node::Manager::setLongPressed() {
    for (uint8_t i = 0; i < this->_maxTouches; i++) {
        Node_SharedPtr pressedNode = this->_pressed[i].lock();
        if (pressedNode != nullptr && pressedNode->isLongPressSupported()) {
            pressedNode->longPress();
            return true;
        }
    }
    return false;
}

void Node::Manager::setLongPressPercentage(const float percentage, const float x, const float y) {
    for (uint8_t i = 0; i < this->_maxTouches; i++) {
        Node_SharedPtr pressedNode = this->_pressed[i].lock();
        if (pressedNode != nullptr && pressedNode->isLongPressSupported()) {
            pressedNode->setLongPressPercentage(percentage, x, y);
            break;
        }
    }
}

void Node::Manager::move(uint8_t index, float x, float y, float dx, float dy) {
    if (this->pressed(index) != nullptr) {
        this->pressed(index)->move(x, y, dx, dy, index);
    }
}

Node_SharedPtr Node::Manager::hovered(const uint8_t index) {
    return this->_hovered[index].lock();
}

void Node::Manager::setHovered(uint8_t index, const Node_SharedPtr& n) {
    if (index >= this->_maxTouches) { return; }
    this->_hovered[index] = n;
}

const Node_WeakPtr& Node::Manager::focused() {
    return this->_focused;
}

void Node::Manager::setFocused(const Node_SharedPtr& n) {
    
    // check if node already focused
    Node_SharedPtr focused = this->_focused.lock();
    if (focused != nullptr && focused == n) { return; }

    // first, check if the node accepts focus
    if (n->focus() == false) return;

    // if focus comes from autofocus request,
    // turn it off to avoir future autofocus conflicts
    n->setWantsAutofocus(false);
    
    if (focused != nullptr) {
        focused->unfocus();
    }
    
    this->_focused = n;
}

void Node::Manager::loseFocus() {
    
    Node_SharedPtr focused = this->_focused.lock();
    if (focused != nullptr) {
        focused->unfocus();
    }
    
    this->_focused.reset();
}

void Node::Manager::unsetAllHoveredAndPressed() {
    for (int i = 0; i < this->_maxTouches; i++) {
        this->_pressed[i].reset();
        if (this->hovered(i) != nullptr) {
            this->hovered(i)->leave();
            this->setHovered(i, nullptr);
        }
    }
}

void Node::Manager::startDragScroll(const Node_SharedPtr& n, uint8_t touchID) {
    if (n == nullptr) { return; } // TEMPORARY CRASH FIX that will go away !!
    if (n->_scrollHandling == ScrollHandling::scrolls && n->_nbTouchScroll == 1) {

        // ignore touch if already scrolling and trying
        // to use another finger.
        if (this->_dragScrolled.lock() == n && this->_dragScrollTouchID != touchID) {
            return;
        }

        this->_dragScrolled = n;
        this->_dragScrollTouchID = touchID;
        this->_dragScrollDeltaY = 0.0f;
        this->_dragScrollDidStart = false;
        this->_dragScrolledReleased = false;
        this->_scrollYVelocityAnimator->reset(0.0f);
    }
}

bool Node::Manager::addDragScrollDelta(Node_SharedPtr node, const float deltaY, const uint8_t touchID) {
    Node_SharedPtr dragScrolled = this->_dragScrolled.lock();

    if (dragScrolled == nullptr) { return false; }
    if (dragScrolled != node) { return false; }
    
    if (touchID != this->_dragScrollTouchID) { return false; }
    
    this->_dragScrollDeltaY += deltaY;
    
    if (this->_dragScrollDidStart == false) {
        // NOTE: this is a way for scrollable node to say "I don't want to scroll right now".
        // Calling scroll function with dx == 0 & dy == 0, just to see if it returns false.
        if (dragScrolled->scroll(0.0f, 0.0f) == false) {
            return false;
        }
        this->_dragScrollDidStart = true;
    }
    
    this->_dragScrollLastDeltaY = _dragScrollDeltaY;
    
    if (this->_dragScrollDidStart) {
        dragScrolled->scroll(0.0f, this->_dragScrollDeltaY);
        this->_dragScrollDeltaY = 0.0f;
   }

    return true;
}

void Node::Manager::releaseDragScroll(uint8_t touchID) {
    if (this->_dragScrollTouchID == touchID) {
        this->_dragScrolledReleased = true;
        this->_scrollYVelocityAnimator->reset(this->_dragScrollLastDeltaY);
    }
}

void Node::Manager::stopDragScroll(const Node_SharedPtr& scrollingNode) {

    Node_SharedPtr dragScrolled = this->_dragScrolled.lock();
    if (scrollingNode != nullptr && scrollingNode != dragScrolled) {
        return;
    }

    this->_dragScrolled.reset();
    this->_dragScrollDeltaY = 0.0f;
    this->_dragScrollDidStart = false;
    this->_dragScrolledReleased = false;
    this->_scrollYVelocityAnimator->reset(0.0f);
}

void Node::Manager::tick(const double dt) {
    Node_SharedPtr dragScrolled = this->_dragScrolled.lock();
    if (dragScrolled != nullptr && this->_dragScrollDidStart && this->_dragScrolledReleased) {
        float speed = this->_scrollYVelocityAnimator->tick(dt);
        dragScrolled->scroll(0.0f, speed);
    }
}

void Node::Manager::didReceiveNotification(const Notification& notification) {
    switch (notification.name) {
        case NotificationName::escKeyUp:
        case NotificationName::softReturnKeyUp:
        case NotificationName::returnKeyUp:
        {
            Node_SharedPtr focused = this->focused().lock();
            if (focused != nullptr) {
                if (focused->getExpectedKeyboard() == KeyboardTypeDoneOnReturn) {
                    this->loseFocus();
                }
            }
            break;
        }
        default:
            break;
    }
}

// ------------------------------
// Node
// ------------------------------

uint32_t Screen::widthInPixels = 100;
uint32_t Screen::heightInPixels = 100;
uint32_t Screen::widthInPoints = 100;
uint32_t Screen::heightInPoints = 100;
float Screen::nbPixelsInOnePoint = 1.0;
bool Screen::isLandscape = false;

Insets Screen::safeAreaInsets = {0.0f, 0.0f, 0.0f, 0.0f};

#ifndef P3S_CLIENT_HEADLESS
// flags for a window to become a simple container, with background color
static ImGuiWindowFlags node_window_flags =
ImGuiWindowFlags_NoTitleBar |
ImGuiWindowFlags_NoScrollbar |
ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoResize |
ImGuiWindowFlags_NoCollapse |
ImGuiWindowFlags_NoNav |
ImGuiWindowFlags_NoDecoration;
// ImGuiWindowFlags_NoBackground
// ImGuiWindowFlags_NoBringToFrontOnFocus
#endif

// --------------------------------------------------
//
// MARK: - Static -
//
// --------------------------------------------------

Node_SharedPtr Node::make() {
    Node_SharedPtr p(new Node);
    p->init(p);
    return p;
}

void Node::disable(const Node_SharedPtr& n) {
    if (n == nullptr) { return; }
    n->setOpacity(0.3f);
    n->setHittable(false);
}

void Node::enable(const Node_SharedPtr& n) {
    if (n == nullptr) { return; }
    n->setOpacity(1.0f);
    n->setHittable(true);
}

void Node::init(const Node_SharedPtr& ref) {
    this->_weakSelf = ref;
}

int Node::nodeID = 0;

#pragma mark - Constructor / Destructor -

#ifdef DEBUG
static int _nodeCount = 0;

int Node::nbNodes() {
    return _nodeCount;
}

void Node::printNbNodes() {
    vxlog_info("NODES: %d", _nodeCount);
}
#endif

// protected
Node::Node() :
_onScrollCallback(nullptr),
_wantsAutofocus(false),
_childrenWantAutofocus(false),
_maxTouches(1) // no multitouch by default
{

#ifdef DEBUG
    ++_nodeCount;
#endif

    Node::nodeID++;

    this->_safeEdges = EdgeMask::all;

    this->_idInt = Node::nodeID;

    // Note: aduermael: we can generate 999,999 IDs without issues.
    // Ideally, should use a pool of ints and recycle them when
    // nodes are destoyed.

    // Note(2): aduermael: IDs should start with '###' not to be displayed
    // by some imgui widgets.
    // https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-why-are-multiple-widgets-reacting-when-i-interact-with-a-single-one-q-how-can-i-have-multiple-widgets-with-the-same-label-or-with-an-empty-label

    size_t max_id_len = 12;
    this->_id = static_cast<char*>(malloc(sizeof(char) * max_id_len));
    snprintf(this->_id, max_id_len, "###%d", this->_idInt);

    this->_parent.reset();
    this->_opacity = 1.0f;
    this->_color = Color(0.0f, 0.0f, 0.0f, 1.0f); // opaque black
    this->_visible = true;

    this->_passthrough = true;
    
    this->_hittable = true;
    this->_hittableThroughParents = true;

    this->_maxWidth = -1.0f;
    this->_maxHeight = -1.0f;

    this->_top = 0.0f;
    this->_left = 0.0f;
    this->_width = 0.0f;
    this->_height = 0.0f;

    this->_sizingPriority.width = SizingPriority::constraints;
    this->_sizingPriority.height = SizingPriority::constraints;

    this->debugName = nullptr;

    this->didResize = nullptr;
    this->parentDidResize = nullptr;
    this->contentDidResize = nullptr;
    this->parentDidResizeSystem = nullptr;
    this->contentDidResizeSystem = nullptr;

    this->_expectedKeyboard = KeyboardTypeNone;
    
    this->_unfocusesFocusedNode = true;

    this->_scrollY = 0.0f;
    this->_scrollHandling = ScrollHandling::dontScroll;
    this->_nbTouchScroll = 1;
}

Node::~Node() {

#ifdef DEBUG
    --_nodeCount;
#endif

    free(this->_id);

    // just in case Node still has children
    this->removeAllChildren();
}


#pragma mark - Public -

void Node::addChild(const Node_SharedPtr& child) {

    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);
    vx_assert(child != nullptr);
    // This is commented because it crashes the app randomly for an unknown reason.
    // (will be debugged after Adrien's PR cleaning the Node API is merged)
    // vx_assert(child->getParent() == nullptr)
    if (child->hasParent()) {
        child->removeFromParent();
    }
    
    _children.push_back(child);
    child->_parent = Node_WeakPtr(self);
    
    if (child->getChildrenWantAutofocus()) {
        self->updateChildrenWantAutofocus();
    }
    
    child->_setHittableThroughParents(_hittable && _hittableThroughParents);

    self->contentDidResizeWrapper();
    child->parentDidResizeWrapper();
}

bool Node::removeChild(const Node_SharedPtr& child) {
    vx_assert(child != nullptr);
    Node_SharedPtr parent = child->_parent.lock();
    bool result = false;
    
    std::vector<Node_SharedPtr>::iterator it = std::find(_children.begin(), _children.end(), child);
    if (it != _children.end()) {
        
        _children.erase(it);

        child->_parent.reset();
        child->_setHittableThroughParents(true);

        result = true;
    }

    if (result == true) {
        this->contentDidResizeWrapper();
    }
    return result;
}

void Node::removeAllChildren() {
    // When this is called from ~Node(), this->_weakSelf is expired/NULL.

    for (const Node_SharedPtr& child : this->_children) {
        child->_parent.reset();
        child->_setHittableThroughParents(true);
    }
    
    this->_children.clear();
}

void Node::removeFromParent() {
    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);

    Node_SharedPtr parent = this->_parent.lock();
    if (parent != nullptr) {
        parent->removeChild(self);
        this->_setHittableThroughParents(true);
    }
}

Node_SharedPtr Node::hitTest(const float x, const float y,
                             float *localX, float *localY,
                             Node_SharedPtr& scrollingNode) {

    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);

    if (this->_hittable == false || this->_hittableThroughParents == false) {
        return nullptr;
    }

    const float bottom = _top - _height;
    const float right = _left + _width;

    // check if x,y in boundaries
    if (x >= this->_left &&
        y >= bottom &&
        x <= right &&
        y <= this->_top) {

        // set scrolling node if needed
        // could be the same as the node returned in the end
        if (this->_scrollHandling == ScrollHandling::scrolls && scrollingNode == nullptr) {
            scrollingNode = self;
        }

        float childLocalX = x - this->_left;
        float childLocalY = y - static_cast<float>(bottom) - this->_scrollY;

        Node_SharedPtr hitChild = nullptr;

        // iterate, from last children to the first one,
        // because last children are in front.
        for (std::vector<Node_SharedPtr>::reverse_iterator it = this->_children.rbegin(); it != this->_children.rend(); ++it) {
            hitChild = (*it)->hitTest(childLocalX, childLocalY, localX, localY, scrollingNode);
            if (hitChild != nullptr) {
                // child hit, return it
                return hitChild;
            }
        }

        if (localX != nullptr) {
            *localX = childLocalX;
        }
        if (localY != nullptr) {
            *localY = childLocalY;
        }

        // no child touched, return self
        if (this->_passthrough == true) {
            return nullptr;
        } else {
            return self;
        }
    }

    // out of boundaries
    return nullptr;
}

void Node::localPoint(const float x, const float y, float& localX, float& localY) {
    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);

    localX = x;
    localY = y;

    Node_SharedPtr parent = self;

    float bottom;
    while (parent != nullptr) {
        bottom = parent->_top - parent->_height;
        localX -= parent->_left;
        localY -= bottom + parent->_scrollY;
        parent = parent->_parent.lock();
    }
}

void Node::globalPoint(const float x, const float y, float& globalX, float& globalY) {
    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);

    globalX = x;
    globalY = y;

    Node_SharedPtr parent = self;
    float bottom;
    while (parent != nullptr) {
        bottom = parent->_top - parent->_height;
        globalX += parent->_left;
        globalY += bottom + parent->_scrollY;
        parent = parent->_parent.lock();
    }
}

void Node::render() {
#ifndef P3S_CLIENT_HEADLESS
    if (_visible == true) {
        Node_SharedPtr self = this->_weakSelf.lock();
        vx_assert(self != nullptr);

        // convert points into pixels
        // /!\ points size being rounded, be sure to handle fullscreen size separately
        const float leftPx = this->_left * Screen::nbPixelsInOnePoint;
        const float topPx = this->_top * Screen::nbPixelsInOnePoint;
        const float widthPx = this->_width == Screen::widthInPoints ? Screen::widthInPixels : this->_width * Screen::nbPixelsInOnePoint;
        const float heightPx = this->_height == Screen::heightInPoints ? Screen::heightInPixels : this->_height * Screen::nbPixelsInOnePoint;
        bool alphaStyleVar = false;
        
        if (heightPx == 0.0f || widthPx == 0.0f) {
            return;
        }

        // NOTE: aduermael: when the chat console has a heightPx of 0,
        // it still renders a rectangle on screen.
        // added the above condition to prevent this, but we should investigate...
//#ifdef DEBUG
//        if (this->debugName != nullptr && strcmp(this->debugName, "console") == 0) {
//            if (heightPx == 0.0f) {
//                vxlog_debug("DEBUG");
//            }
//        }
//#endif

        Node_SharedPtr parent = this->_parent.lock();

        if (parent == nullptr) { // node is the root node

            ImGui::SetNextWindowPos(ImVec2(leftPx, static_cast<float>(Screen::heightInPixels) - topPx),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(widthPx, heightPx), ImGuiCond_Always);

            if (this->_opacity < ImGui::GetStyle().Alpha) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, this->_opacity);
                alphaStyleVar = true;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  ImVec4(this->_color.rf(), this->_color.gf(), this->_color.bf(), this->_color.af()));

            if (!ImGui::Begin(this->_id, nullptr, node_window_flags)) {
                // Early out if the window is collapsed, as an optimization.
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(alphaStyleVar ? 2 : 1);
                ImGui::End();
                return;
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // render children
            for (std::vector<Node_SharedPtr>::iterator it = _children.begin(); it != _children.end(); ++it) {
                (*it)->render();
            }

            if (alphaStyleVar) {
                ImGui::PopStyleVar();
            }

            ImGui::End();

        } else { // not a root node
            const float parentHeightPx =
            static_cast<float>(parent->_height) * Screen::nbPixelsInOnePoint;

            ImGui::SetCursorPos(ImVec2(leftPx, parentHeightPx - topPx));

            if (this->_opacity < ImGui::GetStyle().Alpha) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, this->_opacity);
                alphaStyleVar = true;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                  ImVec4(this->_color.rf(), this->_color.gf(), this->_color.bf(),
                                         this->_color.af()));

            if (!ImGui::BeginChild(this->_id, ImVec2(widthPx, heightPx), false, node_window_flags)) {
                // Early out if the window is collapsed, as an optimization.
                ImGui::PopStyleVar(alphaStyleVar ? 2 : 1);
                ImGui::PopStyleColor();
                ImGui::EndChild();
                return;
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // get visible rows
            this->_scrollY = ImGui::GetScrollY() / Screen::nbPixelsInOnePoint;

            // render children
            for (std::vector<Node_SharedPtr>::iterator it = _children.begin(); it != _children.end(); ++it) {
                (*it)->render();
            }

            if (alphaStyleVar) {
                ImGui::PopStyleVar();
            }

            ImGui::EndChild();
        }
    }
#endif
}

uint8_t Node::getMaxTouches() const {
    return this->_maxTouches;
}

void Node::setMaxTouches(const uint8_t maxTouches) {
    this->_maxTouches = maxTouches;
}

void Node::setWantsAutofocus(const bool autofocus) {
    if (autofocus == this->_wantsAutofocus) return;
    this->_wantsAutofocus = autofocus;
    // Update hierarchy if node is part of one.
    Node_SharedPtr parent = this->_parent.lock();
    if (parent != nullptr) {
        parent->updateChildrenWantAutofocus();
    }
}

bool Node::getWantsAutofocus() const {
    return this->_wantsAutofocus;
}

bool Node::getChildrenWantAutofocus() const {
    return this->_childrenWantAutofocus;
}

void Node::updateChildrenWantAutofocus() {
    bool childrenWantAutoFocus = false;
    
    for (Node_SharedPtr child : this->_children) {
        if (child->getChildrenWantAutofocus() || child->getWantsAutofocus()) {
            childrenWantAutoFocus = true;
            break;
        }
    }
    
    if (childrenWantAutoFocus != this->_childrenWantAutofocus) {
        this->_childrenWantAutofocus = childrenWantAutoFocus;
        Node_SharedPtr parent = this->_parent.lock();
        if (parent != nullptr) {
            parent->updateChildrenWantAutofocus();
        }
    }
}

Node_WeakPtr Node::getChildWantingAutofocusInHierarchy() const {
    Node_SharedPtr self = this->_weakSelf.lock();
    if (self == nullptr) {
        return Node_WeakPtr();
    }
    
    self->_childrenWantAutofocus = false;
    
    for (Node_SharedPtr child : self->_children) {
        if (child->getWantsAutofocus()) {
            return child;
        } else if (child->getChildrenWantAutofocus() == true) {
            return child->getChildWantingAutofocusInHierarchy();
        }
    }
    return Node_WeakPtr();
}

bool Node::hasParent() const {
    return _parent.expired() == false;
}

int Node::getID() const {
    return _idInt;
}

Node_SharedPtr Node::getParent() const {
    return _parent.lock();
}

float Node::getOpacity() const {
    return _opacity;
}

void Node::setOpacity(float opacity) {
    _opacity = opacity;
}

const Color& Node::getColor() const {
    return _color;
}

void Node::setColor(const Color& color) {
    _color = color;
}

bool Node::getVisible() const {
    return _visible;
}

void Node::setVisible(const bool visible) {
    _visible = visible;
}

void Node::setPassthrough(bool b) {
    this->_passthrough = b;
}

void Node::setHittable(bool b) {
    if (this->_hittable == b) return;
    
    this->_hittable = b;
    
    // if _hittableThroughParents is false, _hittableThroughParents is
    // also already false for all children, nothing to do.
    // if _hittableThroughParents is true, propagate value.
    if (this->_hittableThroughParents) {
        for (const Node_SharedPtr& child : this->_children) {
            child->_setHittableThroughParents(b);
        }
    }
}

void Node::_setHittableThroughParents(const bool& b) {
    if (this->_hittableThroughParents == b) return;
    
    this->_hittableThroughParents = b;
    
    // if _hittable is false, _hittableThroughParents is
    // also already false for all children, nothing to do.
    // if _hittable is true, propagate value.
    if (this->_hittable) {
        for (const Node_SharedPtr& child : this->_children) {
            child->_setHittableThroughParents(b);
        }
    }
}

void Node::setSizingPriority(const SizingPriority& width, const SizingPriority& height) {
    this->_sizingPriority.width = width;
    this->_sizingPriority.height = height;
}

void Node::setTop(float f) {
    _top = f;
}

void Node::setLeft(float f) {
    _left = f;
}

void Node::setPos(float left, float top) {
    _left = left;
    _top = top;
}

void Node::setWidth(float f) {
    _width = f;
    if (didResize != nullptr) {
        Node_SharedPtr n = this->_weakSelf.lock();
        if (n != nullptr) {
            didResize(n);
        }
    }
    for (Node_SharedPtr child : _children) {
        child->parentDidResizeWrapper();
    }
    Node_SharedPtr parent = getParent();
    if (parent != nullptr) {
        parent->contentDidResizeWrapper();
    }
}

void Node::setHeight(float f) {
    _height = f;
    if (didResize != nullptr) {
        Node_SharedPtr n = this->_weakSelf.lock();
        if (n != nullptr) {
            didResize(n);
        }
    }
    for (Node_SharedPtr child : _children) {
        child->parentDidResizeWrapper();
    }
    Node_SharedPtr parent = getParent();
    if (parent != nullptr) {
        parent->contentDidResizeWrapper();
    }
}

void Node::setSize(float w, float h) {
    _width = w;
    _height = h;
    if (didResize != nullptr) {
        Node_SharedPtr n = this->_weakSelf.lock();
        if (n != nullptr) {
            didResize(n);
        }
    }
    for (Node_SharedPtr child : _children) {
        child->parentDidResizeWrapper();
    }
    Node_SharedPtr parent = getParent();
    if (parent != nullptr) {
        parent->contentDidResizeWrapper();
    }
}

bool Node::press(const uint8_t &index, const float &x, const float &y) {
    return false;
}

void Node::longPress() {
    // by default, a Node doesn't do anything on a long press
}

void Node::setLongPressPercentage(const float percentage, const float x, const float y) {
    // by default, a Node doesn't do anything on a long press
}

bool Node::isLongPressSupported() {
    // by default, a Node doesn't do anything on a long press
    return false;
}

void Node::release(const uint8_t &index, const float &x, const float &y) {
    // by default, a Node doesn't do anything on release events
}

void Node::cancel(const uint8_t &index) {
    // by default, a Node doesn't do anything on cancel events
}

void Node::hover(const float &x, const float &y, const float &dx, const float &dy) {
    // by default, a Node doesn't do anything on hover events
}

void Node::move(const float &x, const float &y, const float &dx, const float &dy, const uint8_t &index) {
    // by default, a Node doesn't do anything on move events
}

void Node::leave() {
    // by default, a Node doesn't do anything on leave events
}

bool Node::focus() {
    return false;
}

bool Node::scroll(const float& dx, const float& dy) {
    return true;
}

void Node::unfocus() {
    // by default, a node can't get focus, so it never loses focus
}

void Node::didLayout() {
    // nothing done here by default
}

void Node::parentDidResizeWrapper() {
    if (parentDidResize != nullptr || parentDidResizeSystem != nullptr) {
        Node_SharedPtr n = this->_weakSelf.lock();
        if (n != nullptr) {
            if (parentDidResizeSystem != nullptr) {
                parentDidResizeSystem(n);
            }
            if (parentDidResize != nullptr) {
                parentDidResize(n);
            }
        }
    }
    for (const Node_SharedPtr& child : this->_children) {
        child->parentDidResizeWrapper();
    }
}

void Node::contentDidResizeWrapper() {
    if (contentDidResize != nullptr || contentDidResizeSystem != nullptr) {
        Node_SharedPtr n = this->_weakSelf.lock();
        if (n != nullptr) {
            if (contentDidResizeSystem != nullptr) {
                contentDidResizeSystem(n);
            }
            if (contentDidResize != nullptr) {
                contentDidResize(n);
            }
        }
    }
    Node_SharedPtr parent = this->getParent();
    if (parent != nullptr) {
        parent->contentDidResizeWrapper();
    }
}

/// Returns the type of keyboard to use when this Node is focused.
KeyboardType Node::getExpectedKeyboard() {
    return this->_expectedKeyboard;
}

/// Sets the type of keyboard to use when this Node is focused.
void Node::setExpectedKeyboard(const KeyboardType kt) {
    this->_expectedKeyboard = kt;
}

/// Indicates whether a click/touch up on this Node triggers unfocus()
/// on the eventual focused Node.
bool Node::getUnfocusesFocusedNode() const {
    return this->_unfocusesFocusedNode;
}

/// Defines whether a click/touch up on this Node triggers unfocus()
/// on the eventual focused Node.
void Node::setUnfocusesFocusedNode(const bool value) {
    this->_unfocusesFocusedNode = value;
}

Node::ScrollHandling Node::getScrollHandling() const {
    return this->_scrollHandling;
}
