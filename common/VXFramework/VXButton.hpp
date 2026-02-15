//
//  VXButton.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/02/2020.
//

#pragma once

// vx
#include "VXNode.hpp"
#include "VXLabel.hpp"
#include "VXMenuConfig.hpp"
#include "VXMemory.hpp"
#include "VXColor.hpp"
#include "VXNotifications.hpp"

#ifdef P3S_CLIENT_HEADLESS
#include "HeadlessClient_ImGui.h"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include <imgui/bgfx-imgui.h>
#pragma GCC diagnostic pop
#endif

namespace vx {

class ButtonBackground;
typedef std::shared_ptr<ButtonBackground> ButtonBackground_SharedPtr;
typedef std::weak_ptr<ButtonBackground> ButtonBackground_WeakPtr;

class ButtonBackground final : public Node {
    
public:
    bool beveled;
    float bevelSize;
    
    void setColor(const Color& c) override;
    
    static ButtonBackground_SharedPtr make();
    
    ~ButtonBackground() override;
    
    void render() override;
    
private:
    ButtonBackground();
    
    Color _backColor;
    Color _bright;
    Color _dark;
    
#ifndef P3S_CLIENT_HEADLESS
    /// variables required for rendering
    struct RenderVariables {
        float leftPx;
        float topPx;
        float widthPx;
        float heightPx;
        float parentHeightPx;
        float localLeft;
        float localTop;
        float bevelSize;
        Color backColor;
        Color bright;
        Color dark;
    };
    
    struct RenderVariables _renderVars;
#endif
};

class Button;
typedef std::shared_ptr<Button> Button_SharedPtr;
typedef std::weak_ptr<Button> Button_WeakPtr;

class Button : public Node {
    
public:
    
    static Button_SharedPtr make();
    
    /// Factory method
    static Button_SharedPtr makeWithText(const char *text,
                                         const Memory::Strategy strHandlingMode,
                                         const Font::Type& font = Font::Type::body);
    
    ///
    void setColors(MenuConfig::Theme theme);
    
    /// Destructor
    virtual ~Button() override;
    
    ///
    void render() override;
    
    void setSizingPriority(const SizingPriority& width, const SizingPriority& height) override;
    
    /// Sets the Button's background color including hover and press colors
    /// that are computed automatically.
    void setColorWithAutoHoverAndPressed(const Color &color);
    
    // const Color &getColor() const override;
    void setColor(const Color &color) override;
    
    void setOpacity(float opacity) override;
    
    const Color& getColorPressed() const;
    void setColorPressed(const Color& color);
    
    const Color& getColorHovered() const;
    void setColorHovered(const Color& color);
    
    /// sets a text color that is darker than the provided background color
    void setTextColorBasedOnBackground(const Color &backgroundColor);
    
    /// Sets the Button's text color including hover and press colors
    /// that are computed automatically.
    void setTextColorWithAutoHoverAndPressed(const Color &textColor);
    
    const Color &getTextColor() const;
    void setTextColor(const Color& color);
    
    const Color& getTextColorPressed() const;
    void setTextColorPressed(const Color& color);
    
    const Color& getTextColorHovered() const;
    void setTextColorHovered(const Color& color);
    
    const char *getText() const;
    /// Update text (if text label is displayed)
    void setText(const char *text,
                 const Memory::Strategy strHandlingMode);
    
    /**
     * Sets callback for onPress events
     */
    void onPress(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData);
    
    /**
     * Sets callback for onRelease events
     */
    void onRelease(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData);
    
    /**
     * Sets callback for onCancel events
     */
    void onCancel(std::function<void(const Button_SharedPtr&, void *)> callback, void *userData);
    
    bool press(const uint8_t &index, const float &x, const float &y) override;
    void release(const uint8_t &index, const float &x, const float &y) override;
    void cancel(const uint8_t &index) override;
    void hover(const float& x, const float& y, const float& dx, const float& dy) override;
    void leave() override;
    
    /// add/remove bevel
    void setBeveled(const bool b);
    
protected:
    
    ///  Default Constructor
    Button();
    
    /// Updates colors based on _pressed & _hovered
    void _updateColors();
    
    ///
    ButtonBackground_SharedPtr _background;
    
    ///
    Label_SharedPtr _label;
    
private:
    
    /**
     * True when pressed.
     */
    bool _pressed;
    
    /**
     * True when hovered.
     */
    bool _hovered;
    
    /**
     * Used to keep original color and go back to it on release/cancel
     */
    Color _originalColor;
    
    /**
     * Background color when button is pressed.
     */
    Color _colorPressed;
    
    /**
     * Background color when button is hovered.
     * (doesn't work with touch events)
     */
    Color _colorHovered;
    
    /**
     * Text color in normal state
     */
    Color _textColor;
    
    /**
     * Text color when button is pressed.
     */
    Color _textColorPressed;
    
    /**
     * Text color when button is hovered.
     */
    Color _textColorHovered;
    
    /**
     * User data is passed as parameter when triggering callback
     */
    void *_onReleaseUserData;
    
    /**
     * Called when releasing button
     */
    std::function<void(const Button_SharedPtr&, void *)> _onRelease;
    
    /**
     * User data is passed as parameter when triggering callback
     */
    void *_onPressUserData;
    
    /**
     * Called when pressing button
     */
    std::function<void(const Button_SharedPtr&, void *)> _onPress;
    
    /**
     * User data is passed as parameter when triggering callback
     */
    void *_onCancelUserData;
    
    /**
     * Called when releasing outside button (canceling)
     */
    std::function<void(const Button_SharedPtr&, void *)> _onCancel;
};

} // namespace vx
