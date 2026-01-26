//
//  VXScrollBar.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 01/17/23
//

#pragma once

// vx
#include "VXNode.hpp"

namespace vx {

class ScrollBar;
typedef std::shared_ptr<ScrollBar> ScrollBar_SharedPtr;
typedef std::weak_ptr<ScrollBar> ScrollBar_WeakPtr;

class ScrollBar final : public Node {
  public:
    static ScrollBar_SharedPtr make();

    ~ScrollBar() override;
    
    // Used to send updates from the outside regarding content display.
    // When scrolling with mouse wheel for example.
    // displayStart & displayHeight should both be percentages
    void update(const float displayStart, const float displayHeight);

    void setOnMoveCallback(std::function<void(float, void *)> callback, void *userData);

    bool press(const uint8_t &index, const float &x, const float &y) override;
    void release(const uint8_t &index, const float &x, const float &y) override;
    void cancel(const uint8_t &index) override;

    void move(const float &x,
              const float &y,
              const float &dx,
              const float &dy,
              const uint8_t &index) override;

  private:
    ScrollBar();

    //
    void _refreshKnob();
    
    //
    void _callOnMoveCallback();

    // percentage, 0% means displaying top of content
    float _displayStart;
    
    // displayed height percentage
    // Ideally, the knob represents displayed portion of the content,
    // but it also has an absolute minimum height.
    float _displayHeight;

    // prevents the editor scroll to change the bar position
    // while moving the ScrollBar and typing at the same time
    bool _ignoreExternalScroll;
    float _knobDeltaY; // to keep knob on cursor
    
    Node_SharedPtr _knob;

    void *_onMoveUserData;
    std::function<void(float displayStart, void *)> _onMoveCallback;
};

}
