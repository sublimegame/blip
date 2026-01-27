//
//  VXLabel.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/18/20.
//

#pragma once

// Blip
#include "VXAlignment.hpp"
#include "VXFont.hpp"
#include "VXMemory.hpp"
#include "VXNode.hpp"

namespace vx {

class Label;
typedef std::shared_ptr<Label> Label_SharedPtr;
typedef std::weak_ptr<Label> Label_WeakPtr;

class Label final : public Node, public NotificationListener {

public:
    
    /// Factory method
    static Label_SharedPtr make(const char *text,
                                const Memory::Strategy strHandlingMode,
                                const HorizontalAlignment alignment = HorizontalAlignment::center,
                                const Font::Type &font = Font::Type::body);
    
    /// Destructor
    ~Label() override;
    
    void render() override;
    
    const Color &getTextColor() const;
    void setTextColor(const Color &color);
    
    ///
    const char *getText() const;
    void setText(const char *text,
                 const Memory::Strategy strHandlingMode,
                 const bool skipLayout = false);
    
    ///
    const Font::Type& getFontType() const;
    
    ///
    void didReceiveNotification(const vx::Notification& notification) override;
    
private:
    
    /// Constructor
    Label(const char *text,
          const Memory::Strategy strHandlingMode,
          const HorizontalAlignment alignment = HorizontalAlignment::center,
          const Font::Type& font = Font::Type::body);
    
    ///
    void init(const Label_SharedPtr& ref);
    
    ///
    void _refreshIntrisincSize();
    
    char *_text;
    
    Color _textColor;
    
    Font::Type _font;
    HorizontalAlignment _hAlignment;

    Memory::Strategy _strHandlingMode;
    
#ifndef P3S_CLIENT_HEADLESS
    /// variables required for rendering
    struct RenderVariables {
        float leftPx;
        float topPx;
        float widthPx;
        float heightPx;
        float leftMargin;
        float topMargin;
        float parentHeightPx;
        ImVec2 size;
    };

    struct RenderVariables _renderVars;
#endif
};

}
