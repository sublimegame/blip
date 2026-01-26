//
//  VXScrollBar.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 01/17/23
//

#include "VXScrollBar.hpp"
#include "VXMenuConfig.hpp"

#define MIN_KNOB_HEIGHT 40.0f
#define SCROLL_WIDTH 10.0f
#define KNOB_COLOR_IDLE MenuConfig::shared().colorTransparentWhite60
#define KNOB_COLOR_SELECTED MenuConfig::shared().colorTransparentWhite80

using namespace vx;

ScrollBar_SharedPtr ScrollBar::make() {
    std::shared_ptr<ScrollBar> scrollBar(new ScrollBar());
    scrollBar->_weakSelf = scrollBar;

    scrollBar->setColor(MenuConfig::shared().colorTransparentBlack80);
    scrollBar->setSizingPriority(SizingPriority::constraints, SizingPriority::constraints);
    scrollBar->setWidth(SCROLL_WIDTH);

    scrollBar->_knob = Node::make();
    scrollBar->addChild(scrollBar->_knob);
    scrollBar->_knob->setColor(KNOB_COLOR_IDLE);
    scrollBar->_knob->setHittable(false);
    scrollBar->_knob->setWidth(SCROLL_WIDTH);

    return scrollBar;
}

ScrollBar::ScrollBar()
: Node(),
_displayStart(0.0f),
_displayHeight(0.2f),
_ignoreExternalScroll(false),
_knobDeltaY(0.0f),
_knob(nullptr),
_onMoveUserData(nullptr),
_onMoveCallback(nullptr) {
    _passthrough = false;
}

ScrollBar::~ScrollBar() {
    this->removeAllChildren();
    _knob = nullptr;
}

void ScrollBar::update(const float displayStart, const float displayHeight) {
    if (_ignoreExternalScroll) return;
    this->_displayStart = displayStart;
    this->_displayHeight = displayHeight;
    this->_refreshKnob();
}

void ScrollBar::_refreshKnob() {
    float barHeight = getHeight();

    float knobHeight = barHeight * _displayHeight;
    if (knobHeight >= MIN_KNOB_HEIGHT) {
        _knob->setHeight(knobHeight);
    } else {
        _knob->setHeight(MIN_KNOB_HEIGHT);
    }
    _knob->setTop(barHeight * (1.0f - this->_displayStart));
}

void ScrollBar::_callOnMoveCallback() {
    if (this->_onMoveCallback != nullptr) {
        this->_onMoveCallback(this->_displayStart, this->_onMoveUserData);
    }
}

void ScrollBar::setOnMoveCallback(std::function<void(float, void *)> callback,
    void *userData) {
    _onMoveCallback = callback;
    _onMoveUserData = userData;
}

bool ScrollBar::press(const uint8_t &index, const float &x, const float &y) {
    _ignoreExternalScroll = true;

    float knobTop = _knob->getTop();
    float knobBottom = knobTop - _knob->getHeight();

    if (y >= knobBottom && y <= knobTop) {
        _knobDeltaY = y - knobTop;
    } else { // pressing outside of knob: teleport
        float newTop = y + _knob->getHeight() * 0.5f;

        this->_displayStart = 1.0f - (newTop / getHeight());
        if (this->_displayStart < 0.0f) {
            this->_displayStart = 0.0f;
        }
        if (this->_displayStart > (1.0f - this->_displayHeight)) {
            this->_displayStart = 1.0f - this->_displayHeight;
        }

        this->_refreshKnob();
        this->_callOnMoveCallback();

        _knobDeltaY = y - _knob->getTop();
    }
    
    this->_knob->setColor(KNOB_COLOR_SELECTED);
    
    return true; // has to return true so ScrollBar::move can be consumed
}

void ScrollBar::release(const uint8_t &index, const float &x, const float &y) {
    _ignoreExternalScroll = false;
    this->_knob->setColor(KNOB_COLOR_IDLE);
}

void ScrollBar::cancel(const uint8_t &index) {
    _ignoreExternalScroll = false;
    this->_knob->setColor(KNOB_COLOR_IDLE);
}

void ScrollBar::move(const float &x,
                     const float &y,
                     const float &dx,
                     const float &dy,
                     const uint8_t &index) {

    float newTop = y - this->_knobDeltaY;

    this->_displayStart = 1.0f - (newTop / getHeight());
    if (this->_displayStart < 0.0f) {
        this->_displayStart = 0.0f;
    }
    if (this->_displayStart > (1.0f - this->_displayHeight)) {
        this->_displayStart = 1.0f - this->_displayHeight;
    }

    this->_refreshKnob();
    this->_callOnMoveCallback();
}
