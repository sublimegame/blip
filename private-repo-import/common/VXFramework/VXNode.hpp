//
//  VXNode.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 10/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

// NOTE(aduermael): VXNode is used to create UI elements in C++.
// This is legacy code meant to be removed once absolutely all UI is done in Lua.

#pragma once

// C++
#include <list>
#include <map>
#include <vector>
#include <memory>
#include <map>
#include <functional>

// vx
#include "VXColor.hpp"
#include "utils.h"
#include "config.h"
#include "VXInsets.hpp"
#include "VXKeyboard.hpp"
#include "VXTickDispatcher.hpp"
#include "VXValueAnimator.hpp"
#include "VXNotifications.hpp"

// xptools
#include "Macros.h"

namespace vx {

class Screen {
public:
    static uint32_t widthInPixels;
    static uint32_t heightInPixels;
    static uint32_t widthInPoints; // /!\ rounded by integer division, i.e. widthInPoints * density != widthInPixels, be sure to handle fullscreen as a special case and use widthInPixels directly
    static uint32_t heightInPoints;// /!\ rounded by integer division, i.e. heightInPoints * density != heightInPixels, be sure to handle fullscreen as a special case and use heightInPixels directly

    // density reported by device, /!\ this is most of the time NOT an integer value ;
    // which means widthInPoints & heightInPoints being rounded (integer division), can't be used to retrieve fullscreen pixels size
    //
    // note that on some systems like Windows, this value can be customized by the user in their system settings
    //
    // the purpose of using points space is to ensure a consistent aspect of the UI from one device to another,
    // it is not what makes UI elements pixelperfect ; this can only be ensured by the UI framework itself
    // (integer origins & sizes for UI elements, etc.) and UI design (curated metrics)
    static float nbPixelsInOnePoint;

    static bool isLandscape;

    static Insets safeAreaInsets;
};

class Node;
typedef std::shared_ptr<Node> Node_SharedPtr;
typedef std::weak_ptr<Node> Node_WeakPtr;

///
class Node {
    
#pragma mark - public -
    
public:
    
    // A manager for all nodes (only one instance)
    class Manager final : public vx::TickListener, public vx::NotificationListener {
        
    public:
        
        ///
        static Manager *shared();
        
        /// Destructor
        ~Manager() override;
        
        ///
        uint8_t getMaxTouches();
        
        /// Return pressed node for touch index
        Node_SharedPtr pressed(const uint8_t index);
        
        ///
        void setPressed(uint8_t index, const Node_SharedPtr& node);

        ///
        bool setLongPressed();
        void setLongPressPercentage(const float percentage, const float x, const float y);
        
        ///
        void move(uint8_t index, float x, float y, float dx, float dy);
        
        /// Return hovered node for touch index
        Node_SharedPtr hovered(const uint8_t index);
        
        ///
        void setHovered(uint8_t index, const Node_SharedPtr& n);
        
        /// Return focused node
        const Node_WeakPtr& focused();
        
        ///
        void setFocused(const Node_SharedPtr& n);
        
        ///
        void loseFocus();
        
        ///
        void unsetAllHoveredAndPressed();
        
        /**
         * Called when pressing an item than can be drag scrolled.
         * Will stop inertia called for item that's currently scrolling.
         */
        void startDragScroll(const Node_SharedPtr& n, uint8_t touchID);
        
        /**
         * Adds a drag scroll delta for node in parameter.
         * Total delta will be considered next tick to obtain scroll velocity.
         * A threshold is considered for the scroll to actually start.
         * Returns true if addDragScrollDelta results in a movement.
         * (useful to unselect cells)
         */
        bool addDragScrollDelta(Node_SharedPtr node, const float deltaY, const uint8_t touchID);
        
        /**
         * Called when scroll is released, will keep going with inertia
         */
        void releaseDragScroll(uint8_t touchID);
        
        /**
         * Stops drag scroll.
         * If scrollingNode parameter is not NULL, the scroll will
         * stop only if _dragScrolled == scrollingNode
         */
        void stopDragScroll(const Node_SharedPtr& scrollingNode = nullptr);
        
        /**
         * Node::Manager's tick
         */
        void tick(const double dt) override;
        
        /**
         *
         */
        void didReceiveNotification(const Notification& notification) override;

    private:
        
        ///
        static Manager *_instance;
        
        ///
        Manager();
        
        ///
        uint8_t _maxTouches;
        
        /**
         * Nodes being pressed.
         */
        std::vector<Node_WeakPtr> _pressed;
        
        /**
         * Nodes being hovered.
         */
        std::vector<Node_WeakPtr> _hovered;
        
        /**
         * Node being drag scrolled.
         */
        Node_WeakPtr _dragScrolled;
        
        /**
         * Scrolling with what finger?
         */
        uint8_t _dragScrollTouchID;
        
        /**
         * True when drag scrolled item is released, moving with inertia.
         */
        bool _dragScrolledReleased;
        
        /**
         * Drag scroll delta to be applied next tick.
         */
        float _dragScrollDeltaY;
        
        /**
         * Keeping previous delta Y to set speed on release.
         */
        float _dragScrollLastDeltaY;
        
        /**
         * Becomes true when _dragScrollDeltaY goes beyond the threshold
         */
        bool _dragScrollDidStart;
        
        /**
         * Float animator for vertical scroll velocity
         */
        FloatAnimator *_scrollYVelocityAnimator;
        
        /**
         * Node currently with focus. Used for text input only for now.
         * But could be useful for interfaces with no mouse nor touches.
         */
        Node_WeakPtr _focused;
        
        /// Trigger for long press events
        double _longPressDT;
        
        /// Variables to keep track of move delta to cancel long press
        float _longPressDX;
        float _longPressDY;
    };
    
    enum class Attribute {
        none,
        top,
        vcenter,
        bottom,
        left,
        hcenter,
        right,
        width,
        height
    };
    
    // StackAnchor is used to position children nodes
    // at predefined anchors, in stacks.
    // Addding several children at the same anchor will
    // automatically stack them.
    // Removing children from an anchor will automatically
    // rebuild the stack.
    enum StackAnchor {
        stackAnchor_none = 0, // to be used when the child doesn't belong to any parent anchor
        stackAnchor_topLeft = 1,
        stackAnchor_topCenter = 2,
        stackAnchor_topRight = 3,
        stackAnchor_centerLeft = 4,
        stackAnchor_centerCenter = 5,
        stackAnchor_centerRight = 6,
        stackAnchor_bottomLeft = 7,
        stackAnchor_bottomCenter = 8,
        stackAnchor_bottomRight = 9,
        stackAnchor_count = 10, // used to make sure we don't go past the limit
    };
    
    enum class SizingPriority {
        content,
        constraints
    };
    
    enum class ScrollHandling {
        dontScroll,
        scrolls,
        mouseWheelOnly
    };
    
    enum EdgeMask: unsigned char {
        top = 0x01,
        bottom = 0x02,
        left = 0x04,
        right = 0x08,
        none = 0x00,
        all = 0xFF
    };
    
    struct SizingPriorities {
        SizingPriority width;
        SizingPriority height;
    };
    
    /**
     * Factory method.
     * Nodes can only be manipulated as shared pointers.
     * Makes it easier because they can be referenced in different
     * places, like the active view, or within Lua tables.
     */
    static Node_SharedPtr make();
    
    ///
    static void disable(const Node_SharedPtr& n);
    
    ///
    static void enable(const Node_SharedPtr& n);
    
    /**
     * Destructor
     * This has to be "virtual" if class can be derived from.
     */
    virtual ~Node();
    
    /**
     * Adds a child Node
     * stack optional parameter can be used to stack added node with other children.
     */
    virtual void addChild(const Node_SharedPtr& child);
    
    /**
     * Removes a child Node
     * @return whether the child has been found and removed
     */
    virtual bool removeChild(const Node_SharedPtr& child);
    
    /**
     * Removes all children and deletes them.
     */
    virtual void removeAllChildren();
    
    /**
     *
     */
    void removeFromParent();
    
    /**
     * Returns frontmost node colliding with passed coordinates.
     * Also return local x & y if returned node is not null
     * Also provides the first encountered scrolling node.
     * This allows to control scroll with touch events.
     */
    virtual Node_SharedPtr hitTest(const float x, const float y,
                                   float *localX, float *localY,
                                   Node_SharedPtr& scrollingNode);
    
    /**
     * Sets local coords based on given global coords.
     */
    void localPoint(const float x, const float y, float& localX, float& localY);
    
    /**
     * Sets global coords based on given local coords
     */
    void globalPoint(const float x, const float y, float& globalX, float& globalY);
    
    /**
     * Renders Node and all children
     */
    virtual void render();

    ///
    bool hasParent() const;
    
    int getID() const;
    
    ///
    Node_SharedPtr getParent() const;

    float getOpacity() const;
    virtual void setOpacity(float opacity);
    
    const Color &getColor() const;
    virtual void setColor(const Color &color);

    // visible
    bool getVisible() const;
    void setVisible(const bool visible);
    
    void setPassthrough(bool b);
    void setHittable(bool b);
    
    // touch/click events handling
    
    /**
     * Returns true if the Node expects a `release()` to happen next.
     * Receives local position, where node's been pressed.
     * index represents the touch or mouse ID.
     */
    virtual bool press(const uint8_t &index, const float &x, const float &y);
    
    /// Triggered after a long press: keeping pointer down without moving
    virtual void longPress();
    virtual void setLongPressPercentage(const float percentage, const float x, const float y);
    virtual bool isLongPressSupported();
    
    /**
     * Called if action is pressed then released on the same Node
     * index represents the touch or mouse ID.
     */
    virtual void release(const uint8_t &index, const float &x, const float &y);
    
    /**
     * Called if action is pressed but released outside the Node.
     */
    virtual void cancel(const uint8_t &index);
    
    /**
     * Called when hovering node.
     * Called for touch even only when touch is down.
     */
    virtual void hover(const float& x, const float& y, const float& dx, const float& dy);
    
    /**
     * Called when moving mouse or touch, after the node has been pressed.
     * Stops when releasing or cancelling.
     * index represents the touch or mouse ID.
     */
    virtual void move(const float &x, const float &y, const float &dx, const float &dy, const uint8_t &index);
    
    /**
     * Called when leaving node area. (never called for touch events)
     */
    virtual void leave();
    
    /**
     * Returns true if the Node accepts focus.
     */
    virtual bool focus();
    
    /**
     * Called when scrolling.
     * Can return false to prevent node from scrolling.
     */
    virtual bool scroll(const float& dx, const float& dy);
    
    /**
     * Can be called when losing focus.
     */
    virtual void unfocus();
    
    virtual void setSizingPriority(const SizingPriority& width, const SizingPriority& height);

    inline float getTop() { return _top; }
    inline float getLeft() { return _left; }
    inline float getWidth() { return _width; }
    inline float getHeight() { return _height; }

    void setTop(float f);
    void setLeft(float f);
    void setPos(float left, float top);
    void setWidth(float f);
    void setHeight(float f);
    void setSize(float w, float h);

    ///
    SizingPriorities _sizingPriority;

    /**
     * NULL by default
     * give it a name to activate debug for this node
     */
    const char* debugName;

    std::function<void(Node_SharedPtr n)> didResize;
    std::function<void(Node_SharedPtr n)> parentDidResize;
    std::function<void(Node_SharedPtr n)> contentDidResize;
    std::function<void(Node_SharedPtr n)> parentDidResizeSystem;
    std::function<void(Node_SharedPtr n)> contentDidResizeSystem;

    // Calls both parentDidResizeSystem & parentDidResize
    void parentDidResizeWrapper();
    // Calls both contentDidChangeSystem & contentDidResize
    void contentDidResizeWrapper();
    
    /// Returns the type of keyboard to use when this Node is focused.
    KeyboardType getExpectedKeyboard();
    
    /// Sets the type of keyboard to use when this Node is focused.
    void setExpectedKeyboard(const KeyboardType kt);
    
    /// Indicates whether a click/touch up on this Node triggers unfocus()
    /// on the eventual focused Node.
    bool getUnfocusesFocusedNode() const;
    
    /// Defines whether a click/touch up on this Node triggers unfocus()
    /// on the eventual focused Node.
    void setUnfocusesFocusedNode(const bool value);
    
    ///
    ScrollHandling getScrollHandling() const;
    
    ///
    uint8_t getMaxTouches() const;
    
    ///
    void setMaxTouches(const uint8_t maxTouches);
    
    ///
    void setWantsAutofocus(const bool autofocus);
    
    ///
    bool getWantsAutofocus() const;
    
    ///
    bool getChildrenWantAutofocus() const;
    
    /// Returns the first Node found that wants autofocus.
    Node_WeakPtr getChildWantingAutofocusInHierarchy() const;
    
#ifdef DEBUG
    static int nbNodes();
    static void printNbNodes();
#endif
    
protected:
    
    /**
     * Default Constructor
     * @see newNode
     */
    Node();
    
    ///
    void init(const Node_SharedPtr& ref);
    
    /**
     * parent Node of the current Node object
     */
    Node_WeakPtr _parent;
    
    /**
     * children nodes of the current node
     */
    std::vector<Node_SharedPtr> _children;
    
    /// True by defaults, lets touch/click events go through
    bool _passthrough;
    
    /**
     * True by default
     */
    bool _hittable;
    
    /**
     * True by default
     */
    bool _hittableThroughParents;
    
    /**
     * Opacity, 0.0f is full transparent, 1.0f full opaque.
     * Defaults to 1.0f.
     */
    float _opacity;
    
    /**
     * Color of the node
     */
    Color _color;
    
    /**
     * indicates whether the sprite should be rendered or not
     */
    bool _visible;
    
    /** node id, based on nodeID when the Node is created */
    char *_id;
    
    /** integer version of node id*/
    int _idInt;
    
    /**
     * Called when done applying constraints
     */
    virtual void didLayout();
    
    /**
     * Enforces maximum width.
     * Had no effect if < 0
    */
    float _maxWidth;
    
    /**
     * Enforces maximum height.
     * Had no effect if < 0
    */
    float _maxHeight;
    
    // used when moving to a certain part of a Node
    float _scrollY;

    std::function<void()> _onScrollCallback;
    
    /// Indicates if and how the node handles scroll.
    /// Default value is dontScroll
    ScrollHandling _scrollHandling;
    
    /// Number of touches required to scroll
    /// 1 by default
    int _nbTouchScroll;
    
    ///
    /// Weak reference on self to make it possible to call functions
    /// on Node pointers while manipuling shared pointers.
    /// Should not be defined in derived classes,
    /// however it should be set on the factory methods of derived classes
    ///
    Node_WeakPtr _weakSelf;

    float _top;
    float _left;
    float _width;
    float _height;

#pragma mark - private -
    
private:
    
    /// automatically incremented ID for nodes
    static int nodeID;
    
    /// Indicates whether the node wants to be focused by default.
    /// Defaults to false.
    bool _wantsAutofocus;
    
    /// This is set to true when
    bool _childrenWantAutofocus;
    
    ///
    void updateChildrenWantAutofocus();
    
    /// called recursively when calling setHittable
    void _setHittableThroughParents(const bool& b);
    
    /// The type of keyboard to use when this Node is focused.
    KeyboardType _expectedKeyboard;
    
    /// Indicates whether a click/touch up on this Node triggers unfocus()
    /// on the eventual focused Node.
    /// Default value is true.
    bool _unfocusesFocusedNode;
    
    /// Stores which edges are within safe area, based on constraints.
    unsigned short _safeEdges;
    
    /// Number of touches the Node accepts to receive.
    /// This affects its own children nodes.
    uint8_t _maxTouches;
    
    /// Disable copying
    VX_DISALLOW_COPY_AND_ASSIGN(Node)
};

} // namespace vx
