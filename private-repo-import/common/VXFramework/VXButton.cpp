//
//  VXButton.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/02/2020.
//

#include "VXButton.hpp"
#include "VXFont.hpp"
#include "VXAccount.hpp"
#include "VXPrefs.hpp"

#ifndef P3S_CLIENT_HEADLESS
// bgfx+imgui
#include <dear-imgui/imgui.h>
#endif

// xptools
#include "xptools.h"

// engine
#include "inputs.h"

#define VX_BUTTON_BEVEL_SIZE 3.0f
#define VX_BUTTON_PADDING 3.0f

using namespace vx;

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

Button_SharedPtr Button::make() {
    Button_SharedPtr p(new Button);
    p->_weakSelf = p;
    
    p->_background = ButtonBackground::make();
    p->_background->parentDidResize = [](Node_SharedPtr n){
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }
        n->setSize(parent->getWidth(), parent->getHeight());
        n->setPos(0.0f, parent->getHeight());
    };
    p->addChild(p->_background);
        
    return p;
}

Button_SharedPtr Button::makeWithText(const char *text,
                                      const Memory::Strategy strHandlingMode,
                                      const Font::Type &font) {
    Button_SharedPtr button = make();
    
    // add label to button
    button->_label = Label::make(text,
                                 strHandlingMode,
                                 HorizontalAlignment::center,
                                 font);

    button->_label->parentDidResize = [](Node_SharedPtr n){
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }
        n->setTop(parent->getHeight() * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(parent->getWidth() * 0.5f - n->getWidth() * 0.5f);
    };

    button->_label->contentDidResize = [](Node_SharedPtr n){
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }

        const float margin = VX_BUTTON_BEVEL_SIZE + VX_BUTTON_PADDING;

        const float w = margin * 2 + n->getWidth();
        const float h = margin * 2 + n->getHeight();

        if (parent->_sizingPriority.width == SizingPriority::content && parent->getWidth() - w != 0.0f) {
            parent->setWidth(w);
        }
        if (parent->_sizingPriority.height == SizingPriority::content && parent->getHeight() - h != 0.0f) {
            parent->setHeight(h);
        }

        n->setTop(parent->getHeight() * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(parent->getWidth() * 0.5f - n->getWidth() * 0.5f);
    };

    button->addChild(button->_label);
    button->_updateColors();

    button->_label->contentDidResizeWrapper();

    return button;
}

void Button::setColors(MenuConfig::Theme theme) {
    switch(theme) {
        case MenuConfig::Neutral:
            this->setColor(MenuConfig::shared().colorNeutralBackground);
            this->setColorHovered(MenuConfig::shared().colorNeutralBackgroundHovered);
            this->setColorPressed(MenuConfig::shared().colorNeutralBackgroundPressed);
            this->setTextColor(MenuConfig::shared().colorNeutralText);
            this->setTextColorHovered(MenuConfig::shared().colorNeutralTextHovered);
            this->setTextColorPressed(MenuConfig::shared().colorNeutralTextPressed);
            break;
        case MenuConfig::Positive:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorPositiveBackground);
            this->setTextColorBasedOnBackground(MenuConfig::shared().colorPositiveBackground);
            break;
        case MenuConfig::Negative:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorNegativeBackground);
            this->setTextColorBasedOnBackground(MenuConfig::shared().colorNegativeBackground);
            break;
        case MenuConfig::Play:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorPlayBackground);
            this->setColorHovered(MenuConfig::shared().colorPlayBackgroundSelected);
            this->setTextColor(MenuConfig::shared().colorWhite);
            this->setTextColorHovered(MenuConfig::shared().colorWhite);
            this->setTextColorPressed(MenuConfig::shared().colorWhite);
            break;
        case MenuConfig::Build:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorBuildBackground);
            this->setColorHovered(MenuConfig::shared().colorBuildBackgroundSelected);
            this->setTextColor(MenuConfig::shared().colorWhite);
            this->setTextColorHovered(MenuConfig::shared().colorWhite);
            this->setTextColorPressed(MenuConfig::shared().colorWhite);
            break;
        case MenuConfig::Account:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorAccountBackground);
            this->setTextColor(MenuConfig::shared().colorWhite);
            this->setTextColorHovered(MenuConfig::shared().colorWhite);
            this->setTextColorPressed(MenuConfig::shared().colorWhite);
            break;
        case MenuConfig::Disabled:
            this->setColor(MenuConfig::shared().colorDisabledBackground);
            this->setColorHovered(MenuConfig::shared().colorDisabledBackgroundHovered);
            this->setColorPressed(MenuConfig::shared().colorDisabledBackgroundPressed);
            this->setTextColor(MenuConfig::shared().colorDisabledText);
            this->setTextColorHovered(MenuConfig::shared().colorDisabledTextHovered);
            this->setTextColorPressed(MenuConfig::shared().colorDisabledTextPressed);
            break;
        case MenuConfig::System:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorSystemBackground);
            this->setTextColor(MenuConfig::shared().colorWhite);
            this->setTextColorHovered(MenuConfig::shared().colorWhite);
            this->setTextColorPressed(MenuConfig::shared().colorWhite);
            break;
        case MenuConfig::Apple:
            this->setColorWithAutoHoverAndPressed(MenuConfig::shared().colorBlack);
            this->setTextColor(MenuConfig::shared().colorWhite);
            this->setTextColorHovered(MenuConfig::shared().colorWhite);
            this->setTextColorPressed(MenuConfig::shared().colorWhite);
            break;
    }
}

Button::Button() : Node(),
_background(),
_label(),
_pressed(false),
_hovered(false),
_originalColor(MenuConfig::shared().colorNeutralBackground),
_colorPressed(MenuConfig::shared().colorNeutralBackgroundPressed),
_colorHovered(MenuConfig::shared().colorNeutralBackgroundHovered),
_textColor(MenuConfig::shared().colorWhite) {
    
    // buttons to not propagate touch/click events to children
    this->_passthrough = false;
    
    this->_color = MenuConfig::shared().colorTransparent;
    
    this->_textColorHovered = this->_textColor;
    this->_textColorPressed = this->_textColor;
    
    this->_onPress = nullptr;
    this->_onPressUserData = nullptr;
    
    this->_onRelease = nullptr;
    this->_onReleaseUserData = nullptr;
    
    this->_onCancel = nullptr;
    this->_onCancelUserData = nullptr;
    
    this->_sizingPriority.width = SizingPriority::content;
    this->_sizingPriority.height = SizingPriority::content;
    
    // By default, Buttons don't trigger an unfocus.
    this->setUnfocusesFocusedNode(false);
}

Button::~Button() {}

void Button::render() {
#ifndef P3S_CLIENT_HEADLESS
    if (_visible == false) { return; }
    
    Node_SharedPtr self = this->_weakSelf.lock();
    vx_assert(self != nullptr);
    
    // TODO: use local attributes, avoid to allocate vars here
    // convert points into pixels
    const float leftPx = this->_left * Screen::nbPixelsInOnePoint;
    const float topPx = this->_top * Screen::nbPixelsInOnePoint;
    const float widthPx = this->_width * Screen::nbPixelsInOnePoint;
    const float heightPx = this->_height * Screen::nbPixelsInOnePoint;
    bool alphaStyleVar = false;
    
    if (heightPx == 0.0f || widthPx == 0.0f) return;
    
    Node_SharedPtr parent = this->_parent.lock();
    
    if (parent == nullptr) { return; } // all buttons should have a parent
    
    const float parentHeightPx = parent->getHeight() * Screen::nbPixelsInOnePoint;

    ImGui::SetCursorPos(ImVec2(leftPx, parentHeightPx - topPx));
    
    if (this->_hittable == false || this->_hittableThroughParents == false) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.1f);
        alphaStyleVar = true;
    } else if (this->_opacity < ImGui::GetStyle().Alpha) {
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
#endif
}

void Button::setSizingPriority(const SizingPriority& width, const SizingPriority& height) {
    this->_sizingPriority.width = width;
    this->_sizingPriority.height = height;
    if (this->_label != nullptr) {
        this->_label->setSizingPriority(width, height);
        this->_label->contentDidResizeWrapper();
    }
}

void Button::setBeveled(const bool b) {
    this->_background->beveled = b;
}

const Color &Button::getTextColor() const {
    return _label->getTextColor();
}

/// sets a text color that is darker than the provided background color
void Button::setTextColorBasedOnBackground(const Color &backgroundColor) {
    // compute text color
    vx::Color textColor = vx::Color(backgroundColor);
    textColor.applyBrightnessDiff(BUTTON_TEXT_VS_BACKGROUND_COLOR_DIFF);
    this->setTextColorWithAutoHoverAndPressed(textColor);
}

/// Sets the Button's text color including hover and press colors
/// that are computed automatically.
void Button::setTextColorWithAutoHoverAndPressed(const Color &textColor) {
    // set text color
    this->setTextColor(textColor);
    
    // compute text hover color
    vx::Color textHoverColor = vx::Color(textColor);
    textHoverColor.applyBrightnessDiff(BUTTON_TEXTHOVER_VS_TEXT_COLOR_DIFF);
    this->setTextColorHovered(textHoverColor);
    
    // compute text pressed color
    vx::Color textPressedColor = vx::Color(textColor);
    textPressedColor.applyBrightnessDiff(BUTTON_TEXTPRESSED_VS_TEXT_COLOR_DIFF);
    this->setTextColorPressed(textPressedColor);
}

void Button::setTextColor(const Color& color) {
    this->_textColor = color;
    this->_updateColors();
}

const Color& Button::getTextColorPressed() const {
    return this->_textColorPressed;
}

void Button::setTextColorPressed(const Color& color) {
    this->_textColorPressed = color;
    this->_updateColors();
}

const Color& Button::getTextColorHovered() const {
    return this->_textColorHovered;
}

void Button::setTextColorHovered(const Color& color) {
    this->_textColorHovered = color;
    this->_updateColors();
}

///
const char *Button::getText() const {
    if (this->_label == nullptr) {
        return nullptr;
    }
    return this->_label->getText();
}

///
void Button::setText(const char *text,
                     const Memory::Strategy strHandlingMode) {
    if (this->_label != nullptr) {
        this->_label->setText(text, strHandlingMode);
    }
}

/// Sets the Button's background color including hover and press colors
/// that are computed automatically.
void Button::setColorWithAutoHoverAndPressed(const Color &color) {
    // set background color
    this->setColor(color);
    // compute background color for hover
    vx::Color hoverColor = vx::Color(color);
    hoverColor.applyBrightnessDiff(BUTTON_COLOR_HOVER_DIFF);
    this->setColorHovered(hoverColor);
    // compute color for pressed
    vx::Color pressedColor = vx::Color(color);
    pressedColor.applyBrightnessDiff(BUTTON_COLOR_PRESSED_DIFF);
    this->setColorPressed(pressedColor);
}

//const Color &Button::getColor() const {
//    return this->_originalColor;
//}

void Button::setColor(const Color &color) {
    // needed for the vx::Button::getColor() to work correctly
    vx::Node::setColor(color);

    this->_originalColor = color;
    this->_updateColors();
}

void Button::setOpacity(float opacity) {
    this->_opacity = opacity;
    this->_updateColors();
}

const Color& Button::getColorPressed() const {
    return this->_colorPressed;
}

void Button::setColorPressed(const Color& color) {
    this->_colorPressed = color;
    this->_updateColors();
}

const Color& Button::getColorHovered() const {
    return this->_colorHovered;
}

void Button::setColorHovered(const Color& color) {
    this->_colorHovered = color;
    this->_updateColors();
}

bool Button::press(const uint8_t &index, const float &x, const float &y) {
    // ignore mouse right click events
    if (isMouseRightButtonID(index) == true) { return false; }
    
    // haptic feedback
    if (Prefs::shared().canVibrate()) {
        vx::device::hapticImpactLight();
    }
    
    this->_pressed = true;
    this->_updateColors();
    Button_SharedPtr btn = std::dynamic_pointer_cast<Button>(_weakSelf.lock());
    vx_assert(btn != nullptr);
    if (this->_onPress != nullptr) {
        this->_onPress(btn, this->_onPressUserData);
    }
    return true;
}

void Button::release(const uint8_t &index, const float &x, const float &y) {
    this->_pressed = false;
    this->_updateColors();
    Button_SharedPtr btn = std::dynamic_pointer_cast<Button>(_weakSelf.lock());
    vx_assert(btn != nullptr);
    if (this->_onRelease != nullptr) {
        this->_onRelease(btn, this->_onReleaseUserData);
    }
}

void Button::cancel(const uint8_t &index) {
    this->_pressed = false;
    this->_updateColors();
    Button_SharedPtr btn = std::dynamic_pointer_cast<Button>(_weakSelf.lock());
    vx_assert(btn != nullptr);
    if (this->_onCancel != nullptr) {
        this->_onCancel(btn, this->_onCancelUserData);
    }
}

void Button::hover(const float& x, const float& y, const float& dx, const float& dy) {
    this->_hovered = true;
    this->_updateColors();
}

void Button::leave() {
    this->_hovered = false;
    this->_updateColors();
}

void Button::onPress(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData) {
    this->_onPress = callback;
    this->_onPressUserData = userData;
}

void Button::onRelease(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData) {
    this->_onRelease = callback;
    this->_onReleaseUserData = userData;
}

void Button::onCancel(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData) {
    this->_onCancel = callback;
    this->_onCancelUserData = userData;
}

void Button::_updateColors() {
    
    Color _c;
    
    if (this->_pressed) {
        _c = Color(_colorPressed.rf(), _colorPressed.gf(), _colorPressed.bf(), _colorPressed.af() * this->_opacity);
        this->_background->setColor(_c);
        if (_label != nullptr) {
            _label->setTextColor(this->_textColorPressed);
        }
    } else if (this->_hovered) {
        _c = Color(_colorHovered.rf(), _colorHovered.gf(), _colorHovered.bf(), _colorHovered.af() * this->_opacity);
        this->_background->setColor(_c);
        if (_label != nullptr) {
            _label->setTextColor(this->_textColorHovered);
        }
    } else {
        _c = Color(_originalColor.rf(), _originalColor.gf(), _originalColor.bf(), _originalColor.af() * this->_opacity);
        this->_background->setColor(_c);
        if (_label != nullptr) {
            _label->setTextColor(this->_textColor);
        }
    }
}

ButtonBackground_SharedPtr ButtonBackground::make() {
    ButtonBackground_SharedPtr p(new ButtonBackground);
    p->_weakSelf = p;
    return p;
}

ButtonBackground::ButtonBackground() : Node() {
    this->beveled = false;
    this->bevelSize = VX_BUTTON_BEVEL_SIZE;
}
ButtonBackground::~ButtonBackground() {}

void ButtonBackground::setColor(const Color &c) {
    
    this->_backColor = c;
    this->_backColor.makeRoomForBrightnessDiff(BUTTON_BEVELCOLOR_LIGHTER_DIFF * 0.5);
    this->_backColor.makeRoomForBrightnessDiff(BUTTON_BEVELCOLOR_DARKER_DIFF * 0.5);
    
    this->_bright = Color(c);
    this->_bright.applyBrightnessDiff(BUTTON_BEVELCOLOR_LIGHTER_DIFF);
    
    this->_dark = Color(c);
    this->_dark.applyBrightnessDiff(BUTTON_BEVELCOLOR_DARKER_DIFF);
}

void ButtonBackground::render() {
#ifndef P3S_CLIENT_HEADLESS

    _renderVars.leftPx = this->_left * Screen::nbPixelsInOnePoint;
    _renderVars.topPx = this->_top * Screen::nbPixelsInOnePoint;
    _renderVars.widthPx = this->_width * Screen::nbPixelsInOnePoint;
    _renderVars.heightPx = this->_height * Screen::nbPixelsInOnePoint;
    
    const Node_SharedPtr parent = this->_parent.lock();
    
    if (parent != nullptr) { // has to have a parent (the button)
        
        _renderVars.parentHeightPx = parent->getHeight() * Screen::nbPixelsInOnePoint;

        _renderVars.localLeft = _renderVars.leftPx + ImGui::GetWindowPos().x;
        _renderVars.localTop = (_renderVars.parentHeightPx - _renderVars.topPx) + ImGui::GetWindowPos().y;
        
        _renderVars.backColor = _backColor;
        _renderVars.backColor.setAlpha(_renderVars.backColor.af() * ImGui::GetStyle().Alpha);
        
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft, _renderVars.localTop},
                                                  ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.heightPx},
                                                  _renderVars.backColor.u32()
                                                  );

        
        if (this->beveled) {
            
            _renderVars.bevelSize = this->bevelSize * Screen::nbPixelsInOnePoint;
            
            _renderVars.dark = _dark;
            _renderVars.dark.setAlpha(_renderVars.dark.af() * ImGui::GetStyle().Alpha);
            
            _renderVars.bright = _bright;
            _renderVars.bright.setAlpha(_renderVars.bright.af() * ImGui::GetStyle().Alpha);
            
            // top
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft, _renderVars.localTop},
                                                      ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.bevelSize},
                                                      _renderVars.bright.u32()
                                                      );
            
            // right
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft + _renderVars.widthPx - _renderVars.bevelSize, _renderVars.localTop + _renderVars.bevelSize},
                                                      ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.heightPx},
                                                      _renderVars.bright.u32()
                                                      );
            
            // bottom
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft, _renderVars.localTop + _renderVars.heightPx - _renderVars.bevelSize},
                                                      ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.heightPx},
                                                      _renderVars.dark.u32()
                                                      );
            
            // left
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft , _renderVars.localTop + _renderVars.bevelSize},
                                                      ImVec2{_renderVars.localLeft + _renderVars.bevelSize, _renderVars.localTop + _renderVars.heightPx},
                                                      _renderVars.dark.u32()
                                                      );            
            
        }
    }
#endif
}
