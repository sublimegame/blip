//
//  VXLabel.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/18/20.
//

#include "VXLabel.hpp"

// C++
#include <cmath>

#include "xptools.h"

#ifndef P3S_CLIENT_HEADLESS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// imgui + bgfx
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#pragma GCC diagnostic pop
#endif

using namespace vx;

///
Label_SharedPtr Label::make(const char *text,
                            const Memory::Strategy strHandlingMode,
                            const HorizontalAlignment alignment,
                            const Font::Type& font) {
    Label_SharedPtr p(new Label(text, strHandlingMode, alignment, font));
    p->init(p);
    return p;
}

/// Constructor
Label::Label(const char *text,
             const Memory::Strategy strHandlingMode,
             HorizontalAlignment alignment,
             const Font::Type &font)
: Node(), _textColor(Color(1.0f, 1.0f, 1.0f, 1.0f)) {
    this->_font = font;
    this->_hAlignment = alignment;

    this->_strHandlingMode = strHandlingMode;
    switch (_strHandlingMode) {
        case Memory::Strategy::WEAK:
        case Memory::Strategy::TAKE_OWNERSHIP:
            _text = const_cast<char *>(text);
            break;
        case Memory::Strategy::MAKE_COPY:
            _text = string_new_copy(text);
            break;
    }

    this->_sizingPriority.width = SizingPriority::content;
    this->_sizingPriority.height = SizingPriority::content;

    _refreshIntrisincSize();
}

/// Destructor
Label::~Label() {
    NotificationCenter::shared().removeListener(this);
    switch (_strHandlingMode) {
        case Memory::Strategy::WEAK:
            // nothing
            break;
        case Memory::Strategy::TAKE_OWNERSHIP:
        case Memory::Strategy::MAKE_COPY:
            free(_text);
            break;
    }
}

///
const Color &Label::getTextColor() const {
    return this->_textColor;
}

///
void Label::setTextColor(const Color &color) {
    this->_textColor = color;
}

///
const char *Label::getText() const {
    return this->_text;
}

///
void Label::setText(const char *text,
                    const Memory::Strategy strHandlingMode,
                    const bool skipLayout) {
    
    // free existing text if necessary
    switch (_strHandlingMode) {
        case Memory::Strategy::WEAK:
            // nothing
            break;
        case Memory::Strategy::TAKE_OWNERSHIP:
        case Memory::Strategy::MAKE_COPY:
            free(_text);
            break;
    }
    
    // set new text
    this->_strHandlingMode = strHandlingMode;
    switch (_strHandlingMode) {
        case Memory::Strategy::WEAK:
        case Memory::Strategy::TAKE_OWNERSHIP:
            _text = const_cast<char *>(text);
            break;
        case Memory::Strategy::MAKE_COPY:
            _text = string_new_copy(text);
            break;
    }
    
    if (skipLayout) { return; }
    
    _refreshIntrisincSize();
}

const Font::Type& Label::getFontType() const {
    return _font;
}

void Label::didReceiveNotification(const vx::Notification& notification) {
    switch (notification.name) {
        case NotificationName::fontScaleDidChange: {
            _refreshIntrisincSize();
            break;
        }
        default:
            break;
    }
}

void Label::render() {
#ifndef P3S_CLIENT_HEADLESS
    if (this->_visible) {
        Font::shared()->pushImFont(this->_font);
        
        _renderVars.size = ImGui::CalcTextSize(this->_text);
        
        // convert points into pixels
        _renderVars.leftPx = this->_left * Screen::nbPixelsInOnePoint;
        _renderVars.topPx = this->_top * Screen::nbPixelsInOnePoint;
        _renderVars.widthPx = this->_width * Screen::nbPixelsInOnePoint;
        _renderVars.heightPx = this->_height * Screen::nbPixelsInOnePoint;
        
        _renderVars.topMargin = (_renderVars.heightPx - _renderVars.size.y) * 0.5f;
        
        switch (this->_hAlignment) {
            case HorizontalAlignment::left:
                _renderVars.leftMargin = 0.0f;
                break;
            case HorizontalAlignment::center:
                _renderVars.leftMargin = (_renderVars.widthPx - _renderVars.size.x) * 0.5f;
                break;
            case HorizontalAlignment::right:
                _renderVars.leftMargin = (_renderVars.widthPx - _renderVars.size.x);
                break;
        }
        
        const Node_SharedPtr parent = this->_parent.lock();
        
        if (parent == nullptr) { // node is the root node
            ImGui::SetCursorPos(ImVec2(_renderVars.leftPx + _renderVars.leftMargin,
                                       static_cast<float>(Screen::heightInPixels) - _renderVars.topPx + _renderVars.topMargin));
        } else {
            _renderVars.parentHeightPx = parent->getHeight() * Screen::nbPixelsInOnePoint;
            ImGui::SetCursorPos(ImVec2(_renderVars.leftPx + _renderVars.leftMargin, _renderVars.parentHeightPx - _renderVars.topPx + _renderVars.topMargin));
        }
        
        ImGui::SetNextItemWidth(_renderVars.widthPx);
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(_textColor.rf(), _textColor.gf(), _textColor.bf(), _textColor.af()));
        
        ImGui::Text("%s", this->_text);
        
        ImGui::PopStyleColor();

        Font::shared()->popImFont();
    }
#endif
}

void Label::init(const Label_SharedPtr& ref) {
    Node::init(std::static_pointer_cast<Node>(ref));
    NotificationCenter::shared().addListener(this, NotificationName::fontScaleDidChange);
}

void Label::_refreshIntrisincSize() {
    const ImVec2 size = Font::shared()->calcImFontTextSize(_font, _text);
    const float w = ceil(size.x);
    const float h = ceil(size.y);

    bool contentDidChange = false;

    if (w - _width != 0.0f && this->_sizingPriority.width == SizingPriority::content) {
        contentDidChange = true;
        _width = w;
    }

    if (h - _height != 0.0f && this->_sizingPriority.height == SizingPriority::content) {
        contentDidChange = true;
        _height = h;
    }

    if (contentDidChange) {
        this->contentDidResizeWrapper();
    }
}
