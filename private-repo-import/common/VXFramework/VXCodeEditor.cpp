//
//  VXCodeEditor.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/26/20.
//

#include "VXCodeEditor.hpp"

#include "VXAccount.hpp"
#include "VXPrefs.hpp"
#include "VXMenuConfig.hpp"
#include <cmath>
#include <cstring>
#include <utility>
#include <regex>
#include <ctype.h>
#include <chrono>

#include <setjmp.h>

#include "lua.hpp"

// xptools
#include "xptools.h"
#include "textinput.hpp"

// engine
#include "inputs.h"
#include "config.h"

#ifndef P3S_CLIENT_HEADLESS
#include "imgui_internal.h"
#include "imgui.h"
#endif

#define MOVE_SQ_DISTANCE_LIMIT 1.0f
#define BUFFER_SIZE_MARGIN 100000
#define HISTORY_BUFFER_SIZE_MARGIN 64

#define AUTO_COMMIT_TRANSACTION_TIME 750 // ms
#define HISTORY_MAX_TRANSACTIONS 100

#define CODE_CHECK_DELAY 1.0 // seconds

// NOTE: there are 3 different defines for this in the Lua lib
// depending on target. Make sure to use the right one.
// #define LUAI_THROW(L,c)        longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)        if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf        jmp_buf

using namespace vx;

#ifndef P3S_CLIENT_HEADLESS
static ImGuiWindowFlags grid_node_window_flags =
ImGuiWindowFlags_NoTitleBar |
ImGuiWindowFlags_NoScrollbar |
ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoResize |
ImGuiWindowFlags_NoCollapse |
ImGuiWindowFlags_NoNav |
ImGuiWindowFlags_NoDecoration;
#endif

// buf_size: remaining buffer size after cursor
int utf8_char_size(const char* cursor, size_t buf_size) {
    // 80: 10000000
    // C0: 11000000
    // E0: 11100000
    // F0: 11110000
    // F8: 11111000

    if ((cursor[0] & 0x80) == 0) {     // 0xxxxxxx
        return 1;
    } else if ((cursor[0] & 0xE0) == 0xC0) {  // 110xxxxx
        return 2;
    } else if ((cursor[0] & 0xF0) == 0xE0) {  // 1110xxxx
        return 3;
    } else if ((cursor[0] & 0xF8) == 0xF0) {  // 11110xxx
        return 4;
    } else {
        return 0; // Invalid UTF-8 byte
    }
}

CodeEditor_SharedPtr CodeEditor::make(const std::string &str, const Font::Type& font) {
    CodeEditor_SharedPtr p(new CodeEditor);
    p->init(p, str, font);
    return p;
}

CodeEditor_SharedPtr CodeEditor::makeAsTextInput(const std::string &str, const std::string &hint) {
    std::string text = str;
    const Color bgColor = vx::MenuConfig::shared().textInputBackgroundColor;
    const Color focusedBgColor = vx::MenuConfig::shared().textInputBackgroundColorFocused;
    const Color hintColor = vx::MenuConfig::shared().textInputHintColor;
    
    CodeEditor_SharedPtr codeEditor = CodeEditor::make(text, Font::Type::body);
    codeEditor->_padding = vx::MenuConfig::shared().marginVerySmall;
    codeEditor->_borderSize = vx::MenuConfig::shared().textInputBorderSize;
    codeEditor->setPassthrough(false); // required to capture input
    codeEditor->toggleLineNumbers(false);
    codeEditor->toggleSyntaxHighlight(false);
    codeEditor->setNoHighlightTextColor(vx::MenuConfig::shared().colorWhite);
    codeEditor->toggleFocusedColor(true);
    codeEditor->setBackgroundColor(bgColor);
    codeEditor->setFocusedBackgroundColor(focusedBgColor);
    codeEditor->setHintText(hint);
    codeEditor->setHintTextColor(hintColor);
    codeEditor->setColor(vx::MenuConfig::shared().colorTransparent);
    
    codeEditor->refreshBackgroundColors();
    
    return codeEditor;
}

CodeEditor::CodeEditor() :
Node(),
TickListener(),
NotificationListener(),
_text(nullptr),
_previousText(""),
_previousScrollY(0.0f) {}

void CodeEditor::setOnTextChange(std::function<void(const std::string&)> callback) {
    this->_onTextChangeCallback = callback;
}

void CodeEditor::setOnTextError(std::function<void(const std::string&)> callback) {
    this->_onTextErrorCallback = callback;
}

void CodeEditor::setOnScroll(std::function<void()> callback) {
    _onScrollCallback = callback;
}

//void CodeEditor::setOnSelectIssues(std::function<void(const Linter::LuaCheckIssues&)> callback) {
//    _onSelectIssuesCallback = callback;
//}
//
//void CodeEditor::setOnUnselectIssues(std::function<void()> callback) {
//    _onUnselectIssuesCallback = callback;
//}

void CodeEditor::init(const CodeEditor_SharedPtr& p, const std::string &str, const Font::Type& font) {
    
    Node::init(p);
    
    this->_cursor = new Cursor(p);
    this->_codeCheckTimer = 0.0;

    this->_errorEffectDuration = 0.6f;
    this->_errorEffectLeft = 0.0f;
    this->_focused = false;
    
    this->_borderSize = 0.0f;
    this->_padding = 0.0f;
    
    this->_isMovingCursor = false;
    
    this->_sizingPriority.width = SizingPriority::constraints;
    this->_sizingPriority.height = SizingPriority::constraints;
    
    this->_scrollY = 0.0f;
    this->_forceScrollY = false;
    
    this->_scrollHandling = ScrollHandling::scrolls;
    
    this->setExpectedKeyboard(KeyboardTypeReturnOnReturn);
    
    this->_readOnly = false;
    this->_cursorDirty = false;
    
    this->_tabSize = 4;
    
    this->_displayLineNumbers = true;
    this->_useSyntaxHighlight = true;
    this->_noHighlightColor = vx::MenuConfig::shared().colorWhite.u32();
    this->_useFocusColor = false;
    this->_bgColor = vx::MenuConfig::shared().colorTransparent;
    this->_focusedBgColor = vx::MenuConfig::shared().colorTransparent;
    this->_borderColor = vx::MenuConfig::shared().colorTransparent;
    this->_focusedBorderColor = vx::MenuConfig::shared().colorTransparent;
    this->_hintText = "";
    this->_hintTextColor = vx::MenuConfig::shared().colorBackgroundGrey.u32();
    this->_isHintTextActive = false;
    this->_autolower = false;
    
    this->_onTextChangeCallback = nullptr;
    this->_onTextErrorCallback = nullptr;
    this->_onScrollCallback = nullptr;

//    this->_onSelectIssuesCallback = nullptr;
//    this->_onUnselectIssuesCallback = nullptr;

    this->_oneLine = false;
    this->_maxChar = -1;
    this->_regex = "";
    this->_isPassword = false;

    this->_font = font;
    this->setText(str);
    this->_nbLines = std::count(str.begin(), str.end(), '\n') + 1;

    this->_parsedLines = std::vector<ParsedLine>();
    
    this->toggleHint();

    NotificationCenter::shared().addListener(this, NotificationName::fontScaleDidChange);

    this->didResize = [this](Node_SharedPtr n){
        _refreshLineNumber();
        _ensureValidScrollY();
    };
}

void CodeEditor::toggleHint() {
    if (_isHintTextActive && isTextEmpty() == false) {
        _isHintTextActive = false;
    } else if (_isHintTextActive == false && isTextEmpty()) {
        _isHintTextActive = true;
    }
}

void CodeEditor::didReceiveNotification(const vx::Notification &notification) {
    switch (notification.name) {
        case NotificationName::fontScaleDidChange: {
            // _refreshIntrisincSize();
            break;
        }
        default:
            break;
    }
}

CodeEditor::~CodeEditor() {
    delete this->_cursor;
    free(_text);
    NotificationCenter::shared().removeListener(this);
}

void CodeEditor::render() {
    if (this->_visible == false) { return; }
        
    Node_SharedPtr parent = this->_parent.lock();
        
    if (parent == nullptr) { return; } // a code editor can't be a root node
        
    CodeEditor_SharedPtr self = std::dynamic_pointer_cast<CodeEditor>(_weakSelf.lock());
    vx_assert(self != nullptr);
        
#ifndef P3S_CLIENT_HEADLESS
    // convert points into pixels
    _renderVars.leftPx = this->_left * Screen::nbPixelsInOnePoint;
    _renderVars.topPx = this->_top * Screen::nbPixelsInOnePoint;
    _renderVars.widthPx = this->_width * Screen::nbPixelsInOnePoint;
    _renderVars.heightPx = this->_height * Screen::nbPixelsInOnePoint;
        
    _renderVars.paddingPx = this->_padding * Screen::nbPixelsInOnePoint;
    _renderVars.borderPx = this->_borderSize * Screen::nbPixelsInOnePoint;
    _renderVars.paddingAndBorderPx = _renderVars.paddingPx + _renderVars.borderPx;
        
    _renderVars.widthWithoutPaddingAndBorderPx = _renderVars.widthPx - (_renderVars.paddingPx + _renderVars.borderPx) * 2;
    _renderVars.heightWithoutPaddingAndBorderPx = _renderVars.heightPx - (_renderVars.paddingPx + _renderVars.borderPx) * 2;
        
    _renderVars.parentHeightPx = parent->getHeight() * Screen::nbPixelsInOnePoint;

    _renderVars.localLeft = _renderVars.leftPx + ImGui::GetWindowPos().x;
    _renderVars.localTop = (_renderVars.parentHeightPx - _renderVars.topPx) + ImGui::GetWindowPos().y;
        
    ImGui::SetCursorPos(ImVec2(_renderVars.leftPx, _renderVars.parentHeightPx - _renderVars.topPx));

    Font::shared()->pushImFont(this->_font);
        
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        
    // apply error effect if necessary
    if (this->_errorEffectLeft > 0.0f) {
        Color c = MenuConfig::shared().colorNegativeBackground;
        c.lerp(c, this->_color, 1.0f - (this->_errorEffectLeft / this->_errorEffectDuration));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(c.rf(), c.gf(), c.bf(), c.af()));
    } else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImVec4(this->_color.rf(), this->_color.gf(), this->_color.bf(), this->_color.af()));
    }
        
    // background node, includes padding + borders (borders are drawn after)
    ImGui::BeginChild(this->_id,
                      ImVec2(_renderVars.widthPx, _renderVars.heightPx),
                      false,
                      grid_node_window_flags);
        
    ImGui::SetCursorPos(ImVec2(_renderVars.paddingAndBorderPx, _renderVars.paddingAndBorderPx));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        
    // text container
    ImGui::BeginChild("###textcontainer",
                      ImVec2(_renderVars.widthWithoutPaddingAndBorderPx,
                             _renderVars.heightWithoutPaddingAndBorderPx),
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoNavInputs);
        
    // optional focused background color
    if (_useFocusColor) {
        refreshBackgroundColors();
    }

    // Note (september 2024): ImGui obsoleted GetContentRegionMax() and suggested to use the following instead,
    ImVec2 contentSize = {
        ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x - ImGui::GetWindowPos().x,
        ImGui::GetContentRegionAvail().y + ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y
    };
    ImDrawList* drawList = ImGui::GetWindowDrawList();
        
    float topPadding = 0.0f;
    if (this->_oneLine) {
        topPadding = (_renderVars.heightWithoutPaddingAndBorderPx - ImGui::GetTextLineHeight()) * 0.5f;
    }
    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    cursorScreenPos.y += topPadding;
        
    if (this->_forceScrollY) {
        ImGui::SetScrollY(this->_scrollY * Screen::nbPixelsInOnePoint);
        this->_forceScrollY = false;
        if (_onScrollCallback != nullptr) {
            _onScrollCallback();
        }
    }

    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    _charAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing());
        
    int currentLine = maximum(static_cast<int>(floor(_scrollY * Screen::nbPixelsInOnePoint / _charAdvance.y)), 0);
    // auto globalLineMax = (int)mLines.size();
    int lineMax = std::max(0, std::min(static_cast<int>(_parsedLines.size()) - 1, currentLine + static_cast<int>(floor((_scrollY * Screen::nbPixelsInOnePoint + contentSize.y) / _charAdvance.y))));
        
    // optional line numbers
    const float currentLineGutterWidth = getLineGutterWidth();
        
    char buf[255];
    ParsedLine *line;
    float currentLineNumberWidth;
        
    ImVec2 lineStartScreenPos(cursorScreenPos.x, 0.0f /* computed for each line */);
    ImVec2 textScreenPos(lineStartScreenPos.x + currentLineGutterWidth, 0.0f /* computed for each line */);
    ImVec2 textCursorPos;
        
    snprintf(buf, 16, " ");
    const float spaceWidth =
        Font::shared()->calcImFontTextSize(this->_font, buf).x * Screen::nbPixelsInOnePoint;
        
    ImVec2 p1(textScreenPos.x - spaceWidth * 1.5f, 0);
    ImVec2 p2(textScreenPos.x - spaceWidth * 1.5f, 0);
        
    ImU32 errorColor = MenuConfig::shared().colorNegativeBackground.u32();
    ImU32 warningColor = MenuConfig::shared().colorWarningBackground.u32();

    ImU32 lineNumberColor = MenuConfig::shared().colorCodeLineNumber.u32();
    ImU32 commentColor = MenuConfig::shared().colorCodeComment.u32();
    ImU32 selectColor = MenuConfig::shared().colorCodeSelect.u32();
    ImU32 lineColor = MenuConfig::shared().colorCodeDefault.u32();
    ImU32 localColor = MenuConfig::shared().colorCodeLocal.u32();
    ImU32 controlColor = MenuConfig::shared().colorCodeControls.u32();
    ImU32 jumpsColor = MenuConfig::shared().colorCodeJumps.u32();
    ImU32 nilColor = MenuConfig::shared().colorCodeNil.u32();
    ImU32 boolColor = MenuConfig::shared().colorCodeBool.u32();
    ImU32 numberColor = MenuConfig::shared().colorCodeNumber.u32();
    ImU32 stringColor = MenuConfig::shared().colorCodeString.u32();
    ImU32 binaryOpColor = MenuConfig::shared().colorCodeBinaryOp.u32();
    ImU32 mathOpColor = MenuConfig::shared().colorCodeMathOp.u32();
    ImU32 bitwiseOpColor = MenuConfig::shared().colorCodeBitwiseOp.u32();
    ImU32 comparisonOpColor = MenuConfig::shared().colorCodeComparisonOp.u32();
    ImU32 assignationOpColor = MenuConfig::shared().colorCodeAssignationOp.u32();
    ImU32 lengthOpColor = MenuConfig::shared().colorCodeLengthOp.u32();
    ImU32 bracketsColor = MenuConfig::shared().colorCodeBrackets.u32();
        
    ImU32 color;
        
    // iterator on lua tokens
    std::vector<Token>::iterator it;

//    Linter::LuaCheckReport::iterator reportIt;

    size_t drawLen;
        
    const char* end;
        
    char *cStart = this->_cursor->getStart();
    char *cEnd = this->_cursor->getEnd();
    bool selectionInverted = false;
        
    // invert if end is before start
    if (cStart > cEnd) {
        char *tmp = cEnd;
        cEnd = cStart;
        cStart = tmp;
        selectionInverted = true;
    }
        
    bool selection = cStart != cEnd;
        
    if (_parsedLines.empty() == false) {
            
        while (currentLine <= lineMax) {
                
            line = &(_parsedLines.at(currentLine));
                
            // init token iterator
            it = _tokens.begin();
            while (it != _tokens.end() && it->end <= line->start) {
                it++;
            }
                
            // ImVec2 lineStartScreenPos = ImVec2(localLeft, localTop + currentLine * _charAdvance.y);
            lineStartScreenPos.y = cursorScreenPos.y + currentLine * _charAdvance.y;
            textScreenPos.y = lineStartScreenPos.y;
                
            if (selection) {
                if (cStart >= line->start && cStart <= line->end) {
                        
                    if (cEnd == line->end) {
                            
                        p1.x = cursorScreenPos.x + currentLineGutterWidth + (selectionInverted ? _cursor->getEndX() : _cursor->getStartX());
                        p2.x = cursorScreenPos.x + currentLineGutterWidth + (selectionInverted ? _cursor->getStartX() : _cursor->getEndX());
                        p1.y = lineStartScreenPos.y;
                        p2.y = lineStartScreenPos.y + _charAdvance.y;
                        drawList->AddRectFilled(p1, p2, selectColor);
                            
                    } else if (cEnd < line->end) {
                            
                        p1.x = cursorScreenPos.x + currentLineGutterWidth + (selectionInverted ? _cursor->getEndX() : _cursor->getStartX());
                        p2.x = cursorScreenPos.x + currentLineGutterWidth + (selectionInverted ? _cursor->getStartX() : _cursor->getEndX());
                        p1.y = cursorScreenPos.y + _cursor->getStartY();
                        p2.y = cursorScreenPos.y + _cursor->getStartY() + _charAdvance.y;
                        drawList->AddRectFilled(p1, p2, selectColor);
                            
                    } else { // goes beyond end of line, let's draw up to end of line then
                            
                        if (selectionInverted) {
                            p1.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getEndX();
                            p2.x = cursorScreenPos.x + contentSize.x;
                            p1.y = cursorScreenPos.y + _cursor->getEndY();
                            p2.y = cursorScreenPos.y + _cursor->getEndY() + _charAdvance.y;
                            drawList->AddRectFilled(p1, p2, selectColor);
                        } else {
                            p1.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getStartX();
                            p2.x = cursorScreenPos.x + contentSize.x;
                            p1.y = cursorScreenPos.y + _cursor->getStartY();
                            p2.y = cursorScreenPos.y + _cursor->getStartY() + _charAdvance.y;
                            drawList->AddRectFilled(p1, p2, selectColor);
                        }
                    }
                        
                } else if (cEnd >= line->start && cEnd <= line->end) {
                        
                    if (selectionInverted) {
                            
                        p1.x = cursorScreenPos.x + currentLineGutterWidth;
                        p2.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getStartX();
                        p1.y = cursorScreenPos.y + _cursor->getStartY();
                        p2.y = cursorScreenPos.y + _cursor->getStartY() + _charAdvance.y;
                        drawList->AddRectFilled(p1, p2, selectColor);
                            
                    } else {
                            
                        p1.x = cursorScreenPos.x + currentLineGutterWidth;
                        p2.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getEndX();
                        p1.y = cursorScreenPos.y + _cursor->getEndY();
                        p2.y = cursorScreenPos.y + _cursor->getEndY() + _charAdvance.y;
                        drawList->AddRectFilled(p1, p2, selectColor);
                            
                    }
                        
                } else if (cStart < line->start && cEnd >= line->end) { // whole line if within selection
                        
                    p1.x = cursorScreenPos.x + currentLineGutterWidth;
                    p2.x = cursorScreenPos.x + contentSize.x;
                    p1.y = lineStartScreenPos.y;
                    p2.y = lineStartScreenPos.y + _charAdvance.y;
                    drawList->AddRectFilled(p1, p2, selectColor);
                        
                }
            }
                
            // DRAW LINE NUMBER (right aligned)
             
//            if (line->isFirstPart) {
//                reportIt = _luaCheckReport.find(static_cast<int>(line->lineNo));
//                if (reportIt != _luaCheckReport.end()) {
//                    p1.x = lineStartScreenPos.x;
//                    p2.x = p1.x + currentLineGutterWidth - _charAdvance.x;
//                    p1.y = lineStartScreenPos.y;
//                    p2.y = p1.y + _charAdvance.y;
//                    drawList->AddRectFilled(p1, p2, reportIt->second.nErrors > 0 ? errorColor : warningColor);
//                }
//            }

            if (_displayLineNumbers && line->isFirstPart) {
                snprintf(buf, 16, "%zu  ", line->lineNo);
                currentLineNumberWidth =
                    Font::shared()->calcImFontTextSize(this->_font, buf).x * Screen::nbPixelsInOnePoint;
                drawList->AddText(ImVec2(lineStartScreenPos.x + currentLineGutterWidth - currentLineNumberWidth, lineStartScreenPos.y), lineNumberColor, buf);
            }
                
            // DRAW LINE
                
            // if syntax highlighting is disabled
            if (_useSyntaxHighlight == false) {
                if (_isHintTextActive == false) {
                    if (this->_isPassword) {
                        drawList->AddText(textScreenPos, _noHighlightColor , _passwordText.c_str(), _passwordText.c_str() + _passwordText.size());
                    } else {
                        drawList->AddText(textScreenPos, _noHighlightColor , line->start, line->end);
                    }
                } else {
                    drawList->AddText(textScreenPos, _hintTextColor, _hintText.c_str(), _hintText.c_str() + _hintText.size());
                }
            }
            // or if next token is after current line, just draw the line (comment)
            else if (it == _tokens.end() || it->start >= line->end) {
                drawList->AddText(textScreenPos, commentColor, line->start, line->end);
            }
                
            // consider each token in current line
            else {
                const char* cursor = line->start;
                textCursorPos = textScreenPos;
                    
                while (cursor < line->end) {
                        
                    if (it == _tokens.end() || it->start > cursor) {
                            
                        if (it == _tokens.end() || it->start >= line->end) { // last token reached
                                
                            drawList->AddText(textCursorPos, commentColor, cursor, line->end);
                                
                            cursor = line->end; // go to next line
                                
                        } else { // outside token, but another token starts later on same line
                                
                            drawLen = it->start - cursor;

                            if (_tokenTypeToTokenGroup(it->type) == tokenGroup::string) {
                                color = stringColor;
                            } else {
                                color = commentColor;
                            }

                            drawList->AddText(textCursorPos, color, cursor, it->start);

                            snprintf(buf, drawLen + 1, "%s", cursor);
                                
                            textCursorPos.x += Font::shared()->calcImFontTextSize(this->_font, buf).x * Screen::nbPixelsInOnePoint;
                                
                            cursor = it->start;
                        }

                    } else { // within token
                            
                        // use right color depending on token group
                        switch (_tokenTypeToTokenGroup(it->type)) {
                            case tokenGroup::local:
                                color = localColor;
                                break;
                            case tokenGroup::controlBlock:
                                color = controlColor;
                                break;
                            case tokenGroup::jumps:
                                color = jumpsColor;
                                break;
                            case tokenGroup::nil:
                                color = nilColor;
                                break;
                            case tokenGroup::boolean:
                                color = boolColor;
                                break;
                            case tokenGroup::name:
                                color = numberColor;
                                break;
                            case tokenGroup::number:
                                color = numberColor;
                                break;
                            case tokenGroup::string:
                                color = stringColor;
                                break;
                            case tokenGroup::binaryOp:
                                color = binaryOpColor;
                                break;
                            case tokenGroup::mathOp:
                                color = mathOpColor;
                                break;
                            case tokenGroup::bitwiseOp:
                                color = bitwiseOpColor;
                                break;
                            case tokenGroup::comparisonOp:
                                color = comparisonOpColor;
                                break;
                            case tokenGroup::assignationOp:
                                color = assignationOpColor;
                                break;
                            case tokenGroup::lengthOp:
                                color = lengthOpColor;
                                break;
                            case tokenGroup::brackets:
                                color = bracketsColor;
                                break;
                            case tokenGroup::comment:
                                color = commentColor;
                                break;
                            case tokenGroup::other:
                                color = lineColor;
                                break;
                        }
                            
                        // end of line or it
                        end = it->end > line->end ? line->end : it->end;
                            
                        drawLen = end - cursor;
                            
                        drawList->AddText(textCursorPos, color, cursor, end);
                            
                        snprintf(buf, drawLen + 1, "%s", cursor);
                            
                        textCursorPos.x += Font::shared()->calcImFontTextSize(this->_font, buf).x * Screen::nbPixelsInOnePoint;
                            
                        cursor = end;
                            
                        // go on to next token if there is one
                        if (end == it->end) {
                            it++;
                        }
                    }
                }
            }
                
            ++currentLine;
        }
    }
        
    // DRAW CURSORS
        
    if (this->_focused) {
            
        if (cEnd != cStart) {
            if (this->_focused) {
                p1.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getStartX() - 2.0f * Screen::nbPixelsInOnePoint;
                p2.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getStartX();
                p1.y = cursorScreenPos.y + _cursor->getStartY() ;
                p2.y = cursorScreenPos.y + _cursor->getStartY() + _charAdvance.y;
                drawList->AddRectFilled(p1, p2, localColor);
            }
        }
            
        if (this->_focused && _cursorDT <= MenuConfig::shared().cursorBlinkCycle * 0.5) {
            p1.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getEndX() - 2.0f * Screen::nbPixelsInOnePoint;
            p2.x = cursorScreenPos.x + currentLineGutterWidth + _cursor->getEndX();
            p1.y = cursorScreenPos.y + _cursor->getEndY();
            p2.y = cursorScreenPos.y + _cursor->getEndY() + _charAdvance.y;
            drawList->AddRectFilled(p1, p2, stringColor);
        }
    }
        
    // add dummy to enforce content height and get the node to scroll
    ImGui::Dummy(ImVec2(0.0f, _parsedLines.size() * this->_charAdvance.y));
        
    ImGui::EndChild(); // text container
    ImGui::PopStyleColor(); // transparent background
        
    // BORDERS
    if (this->_borderSize > 0.0f) {
            
        _renderVars.borderColor = (_focused ? _focusedBorderColor : _borderColor).u32();
            
        // top
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft, _renderVars.localTop},
                                                  ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.borderPx},
                                                  _renderVars.borderColor
                                                  );
            
        // right
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft + _renderVars.widthPx - _renderVars.borderPx, _renderVars.localTop + _renderVars.borderPx},
                                                  ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.heightPx},
                                                  _renderVars.borderColor
                                                  );

        // bottom
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft, _renderVars.localTop + _renderVars.heightPx - _renderVars.borderPx},
                                                  ImVec2{_renderVars.localLeft + _renderVars.widthPx, _renderVars.localTop + _renderVars.heightPx},
                                                  _renderVars.borderColor
                                                  );

        // left
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2{_renderVars.localLeft , _renderVars.localTop + _renderVars.borderPx},
                                                  ImVec2{_renderVars.localLeft + _renderVars.borderPx, _renderVars.localTop + _renderVars.heightPx},
                                                  _renderVars.borderColor
                                                  );
    }
        
    ImGui::EndChild(); // background (includes padding)
        
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    Font::shared()->popImFont();
        
#endif
}

void CodeEditor::setOneLine(bool value) {
    _oneLine = value;
//    _refreshIntrisincSize();
}
void CodeEditor::setMaxChar(const uint32_t value) { _maxChar = value; }
void CodeEditor::setRegex(std::string value) { _regex = value; }
void CodeEditor::setIsPassword(bool value) {
    this->_isPassword = value;
    if (this->_isPassword) {
        this->setPasswordText(std::string(this->_text));
    }
}
bool CodeEditor::getIsPassword() { return this->_isPassword; }

// returns the total height in points
float CodeEditor::getTotalHeight() {
    return static_cast<float>(_parsedLines.size()) * Font::shared()->getSizeForFontType(DEARIMGUI_FONT, _font) /
           Screen::nbPixelsInOnePoint;
}

float CodeEditor::getViewHeight() {
    return _height;
}

void CodeEditor::triggerErrorEffect() {
    this->_errorEffectLeft = this->_errorEffectDuration;
}

void CodeEditor::saveState() {
    // save entire code editor text
    _previousText = std::string(_text);
    _previousScrollY = _scrollY;
}

void CodeEditor::restoreState() {
    // copy _previousText into _text
    free(_text);
    _text = nullptr;
    _text_allocated = BUFFER_SIZE_MARGIN;
    _text = static_cast<char *>(malloc(sizeof(char) * this->_text_allocated));
    if (_text == nullptr) {
        return;
    }
    this->increaseTextBufferToFitIfNecessary(_previousText.size() + 1); // + 1 for null termination
    strcpy(_text, _previousText.c_str());

    _scrollY = _previousScrollY;
    _ensureValidScrollY();
}

void CodeEditor::tick(const double dt) {
    
    _cursorDT += dt;
    if (_cursorDT > MenuConfig::shared().cursorBlinkCycle) {
        _cursorDT = 0.0;
    }
    
    if (_cursorDirty) {
        ensureCursorVisible();
        _cursorDirty = false;
    }
    
    if (this->_errorEffectLeft > 0.0f) {
        this->_errorEffectLeft -= static_cast<float>(dt);
        if (this->_errorEffectLeft < EPSILON_ZERO) {
            this->_errorEffectLeft = 0.0f;
        }
    }

    if (_codeCheckTimer > 0.0) {
        _codeCheckTimer -= dt;
        if (_codeCheckTimer <= 0.0) {
//            _luaCheckReport = Linter::luaCheck(std::string(this->_text));
            refreshSelectedIssue();
        }
    }
}

void CodeEditor::onTextChange(const std::string& oldText) {
    
    // test if oldText != this->getText()
    
    size_t len = strlen(this->_text) - 1; // -1 to get rid of ending \n
    
    if (this->_maxChar != static_cast<uint32_t>(-1) && static_cast<uint32_t>(len) > this->_maxChar) {
        
        // error effect & callback
        this->_errorEffectLeft = this->_errorEffectDuration;
        if (_onTextErrorCallback != nullptr) {
            _onTextErrorCallback(this->getText());
        }
        
        // roll back to previous text
        setText(oldText);
        return;

    } else if (this->_regex.empty() == false && isTextEmpty() == false) {
        // apply comparison to the regex if any
        std::string regex_expr = "(" + this->_regex + ")|(^$)"; // allow empty string
        
        if (std::regex_match(this->_text, this->_text + len, std::regex(regex_expr)) == false) {
            
            // error effect & callback
            this->_errorEffectLeft = this->_errorEffectDuration;
            if (_onTextErrorCallback != nullptr) {
                _onTextErrorCallback(this->getText());
            }
            
            // roll back to previous text
            setText(oldText);
            return;
        }
    }
    
    this->toggleHint();
    
    if (_onTextChangeCallback != nullptr) {
        _onTextChangeCallback(this->getText());
    }
    
    if (this->getIsPassword()) {
        this->setPasswordText(std::string(this->_text));
    }

    this->_refreshLineNumber();
}

char* CodeEditor::pointToCursorPointer(float x, float y) {
    
#ifndef P3S_CLIENT_HEADLESS
    // optional line numbers
    const float currentLineGutterWidth = getLineGutterWidth();
    const float paddingPx = this->_padding * Screen::nbPixelsInOnePoint;
    const float borderPx = this->_borderSize * Screen::nbPixelsInOnePoint;
    const float paddingAndBorderPx = paddingPx + borderPx;
    
    int32_t line = 0;
    // if _oneLine, line is always 0
    if (this->_oneLine == false) {
        float invertedY = (static_cast<float>(_height) - y - _padding - _borderSize) * Screen::nbPixelsInOnePoint;
        if (_scrollY < 0.0f) {
            invertedY -= _scrollY;
        }
        // invertedY < 0 means in padding area -> first line
        if (invertedY >= 0) {
            line = static_cast<int32_t>(floor((invertedY) / _charAdvance.y));
        }
    }
    float localX = x * Screen::nbPixelsInOnePoint - currentLineGutterWidth - paddingAndBorderPx;
    
    //    vxlog_trace("----");
    //    vxlog_trace("nbPixelsInOnePoint: %.2f", Screen::nbPixelsInOnePoint);
    //    vxlog_trace("_charAdvance.y: %.2f", _charAdvance.y);
    //    vxlog_trace("LINE: %d", line);
    //    vxlog_trace("Y: %.2f", y);
    //    vxlog_trace("HEIGHT: %.2f", this->_height.value());
    //    vxlog_trace("invertedY: %.2f", invertedY);
    //    vxlog_trace("SCROLL Y: %.2f", _scrollY);
    //    vxlog_trace("INVERTED - SCROLL: %.2f", invertedY - _scrollY);
    //    vxlog_trace("----");
    
    // optional hint text is cleared once before processing inputs, if focused
    // cursor is placed at the beginning automatically, we can skip here
    if (_isHintTextActive) {
        return nullptr;
    }
    
    // place cursor at the end of last line by default
    const int32_t nbLines = static_cast<int32_t>(_parsedLines.size());
    if (nbLines == 0) { return _text; }
    vx_assert(nbLines > 0);
    
    if (line < 0 || line >= nbLines) {
        line = maximum(nbLines - 1, 0);
        localX = static_cast<float>(Screen::widthInPixels + 1);
    }
    
    float lineWidth = Font::shared()->calcImFontTextSize(this->_font, _parsedLines.at(line).start,
                                                         _parsedLines.at(line).end).x * Screen::nbPixelsInOnePoint;
    
    char* cursor = _parsedLines.at(line).start;
    const char* lineEnd = _parsedLines.at(line).end;
    unsigned int c;
    
    if (localX >= lineWidth) {
        
        // go to end of line
        while (cursor < lineEnd) {
            c = static_cast<unsigned int>(*cursor);
            if (c < 0x80) {
                cursor += 1;
            } else {
                cursor += ImTextCharFromUtf8(&c, cursor, lineEnd);
                if (c == 0) // Malformed UTF-8?
                    break;
            }
        }

        // vxlog_info("LINE: %d - lineLen: %d - line chars: %d - CURSOR on LINE: %d", line, lineLen, _parsedLines.at(line).chars, cursor - _parsedLines.at(line).start);

        return cursor;

    } else if (localX < 0.0f) {
        
        return cursor; // line start
        
    } else {
        
        float totalWidth = 0.0f;
        char* previousCursor;
        
        while (cursor < lineEnd) {
            
            previousCursor = cursor;
            
            c = static_cast<unsigned int>(*cursor);
            if (c < 0x80) {
                cursor += 1;
            } else {
                cursor += ImTextCharFromUtf8(&c, cursor, lineEnd);
                if (c == 0) // Malformed UTF-8?
                    break;
            }
            
            const float char_width = Font::shared()->getImFontCharAdvanceX(this->_font, c);
            
            totalWidth += char_width;
            
            // just went past localX
            if (totalWidth >= localX) {
                // use current cursor if past half the width of the char
                if (totalWidth - localX <= char_width * 0.5f) {
                    return cursor;
                } else { // use previous position otherwise
                    return previousCursor;
                }
            }
        }
        
        return cursor;
    }
#else
    return nullptr;
#endif
}

void CodeEditor::toggleLineNumbers(bool value) {
    _displayLineNumbers = value;
}

void CodeEditor::toggleSyntaxHighlight(bool value) {
    _useSyntaxHighlight = value;
}

void CodeEditor::setNoHighlightTextColor(const Color& c) {
    _noHighlightColor = c.u32();
}

void CodeEditor::toggleFocusedColor(bool value) {
    _useFocusColor = value;
}

void CodeEditor::setBackgroundColor(const Color& c) {
    _bgColor = c;
    
    // pre-blend borderColor to replicate the look of TextInput w/ 4-quads-border on top of bgColor
    // instead here, we have 1 quad under and beyond background quad
    _borderColor = Color(vx::MenuConfig::shared().textInputBorderColor);
    _borderColor.set(LERP(_borderColor.rf(), c.rf(), _borderColor.af()),
                     LERP(_borderColor.gf(), c.gf(), _borderColor.af()),
                     LERP(_borderColor.bf(), c.bf(), _borderColor.af()),
                     c.af());
}

void CodeEditor::setFocusedBackgroundColor(const Color& c) {
    _focusedBgColor = c;
    
    // pre-blend borderColor to replicate the look of TextInput w/ 4-quads-border on top of bgColor
    // instead here, we have 1 quad under and beyond background quad
    _focusedBorderColor = Color(vx::MenuConfig::shared().textInputBorderColor);
    _focusedBorderColor.set(LERP(_focusedBorderColor.rf(), c.rf(), _focusedBorderColor.af()),
                            LERP(_focusedBorderColor.gf(), c.gf(), _focusedBorderColor.af()),
                            LERP(_focusedBorderColor.bf(), c.bf(), _focusedBorderColor.af()),
                            c.af());
}

void CodeEditor::setPasswordText(const std::string& str) {
    _passwordText = std::string(str.length() - 1, '*'); // -1 because str always has an ending \n
}

void CodeEditor::setHintText(const std::string& value) {
    _hintText = value;
}

void CodeEditor::setHintTextColor(const Color &c) {
    _hintTextColor = c.u32();
}

bool CodeEditor::press(const uint8_t &index, const float &x, const float &y) {
    if (isTouchEventID(index) == true) {
        // touch event
        this->_setPointerOnrelease = true;
        this->_moveX = 0.0f;
        this->_moveY = 0.0f;
        this->_longPressX = x;
        this->_longPressY = y;
    } else {
        // mouse click event
        this->_isMovingCursor = true;
        this->_cursor->setPosition(x, y, false);
        _cursorDirty = true;
        // vx::textinput::textInputRequest will be called when focusing
        if (this->_focused) {
            vx::textinput::textInputUpdate(std::string(this->_text), this->_cursor->getStart() - this->_text, this->_cursor->getEnd() - this->_text);
        }
    }
    
    return true;
}

void CodeEditor::longPress() {
    if (this->_isMovingCursor == false) {
        if (Prefs::shared().canVibrate()) {
            vx::device::hapticImpactLight();
        }
        this->_setPointerOnrelease = false;
        this->_isMovingCursor = true;
        this->_cursor->setPosition(this->_longPressX, this->_longPressY, false);
        _cursorDirty = true;
        vx::textinput::textInputUpdate(std::string(this->_text), this->_cursor->getStart() - this->_text, this->_cursor->getEnd() - this->_text);
    }
}

void CodeEditor::setLongPressPercentage(const float percentage, const float x, const float y) {
    // do nothing
}

bool CodeEditor::isLongPressSupported() {
    return true;
}

void CodeEditor::move(const float &x, const float &y, const float &dx, const float &dy, const uint8_t &index) {
    if (this->_isMovingCursor) {
        this->_focused = true; // to display cursors
        this->_cursor->setPosition(x, y, true);
        _cursorDirty = true;
        vx::textinput::textInputUpdate(std::string(this->_text), this->_cursor->getStart() - this->_text, this->_cursor->getEnd() - this->_text);
    }
}

void CodeEditor::release(const uint8_t &index, const float &x, const float &y) {
    if (this->_setPointerOnrelease) {
        this->_cursor->setPosition(x, y, false);
        _cursorDirty = true;
        vx::textinput::textInputUpdate(std::string(this->_text), this->_cursor->getStart() - this->_text, this->_cursor->getEnd() - this->_text);
    }
    this->_isMovingCursor = false;
}

void CodeEditor::cancel(const uint8_t &index) {
    
    this->_isMovingCursor = false;
}

bool CodeEditor::scroll(const float& dx, const float& dy) {
    if (this->_setPointerOnrelease && fabsf(dy) > MOVE_SQ_DISTANCE_LIMIT) { this->_setPointerOnrelease = false; }
    _scrollY += dy;
    _ensureValidScrollY();
    return _isMovingCursor == false;
}

// schedule vertical movement
void CodeEditor::goToStartPercentage(const float p) {
    _scrollY = p * this->getTotalHeight();
    _ensureValidScrollY();
}

float CodeEditor::getStartPercentage() {
    float totalHeight = this->getTotalHeight();
    if (totalHeight == 0.0f) return 0.0f;
    return _scrollY / totalHeight;
}

float CodeEditor::getViewPercentage() {
    if (this->_parsedLines.empty()) { this->refreshLineParts(); }
    float totalHeight = this->getTotalHeight();
    if (totalHeight == 0.0f) return 0.0f;
    return getViewHeight() / totalHeight;
}

bool CodeEditor::focus() {
    if (this->_setPointerOnrelease == false && this->_isMovingCursor == false) return false;
    
    this->_focused = true;

    vx::textinput::textInputRequest(std::string(this->_text), this->_cursor->getStart() - this->_text, this->_cursor->getEnd() -  this->_text, true, TextInputKeyboardType_Default, TextInputReturnKeyType_Default, false);
    return true;
}

void CodeEditor::unfocus() {
    this->_focused = false;
    vx::textinput::textInputAction(TextInputAction_Close);
}

typedef struct LoadS {
    const char *s;
    size_t size;
} LoadS;

static const char *getS(lua_State *L, void *ud, size_t *size) {
    LoadS *ls = static_cast<LoadS *>(ud);
    (void)L;  /* not used */
    if (ls->size == 0) return nullptr;
    *size = ls->size;
    ls->size = 0;
    return ls->s;
}

void CodeEditor::setText(const std::string &str) {

    // for DEBUG:
    // std::string str = "hello world \"❤️\"\nHERE IS A SHIRT \"👕\"\nthis is a test\n\"🙂🔥\"\nunsupported emoji: - \"🤢\" -\ndone\n";

    if (this->_text == nullptr) {
        this->_text_allocated = BUFFER_SIZE_MARGIN;
        this->_text = static_cast<char*>(malloc(sizeof(char) * this->_text_allocated));
        if (this->_text == nullptr) {
            return;
        }
    }
    
    size_t len = strlen(str.c_str());
    
    this->increaseTextBufferToFitIfNecessary(len + 1); // + 1 for null termination
    
    memcpy(this->_text, str.c_str(), len);
    this->_text[len] = '\0';

    this->_refreshLineNumber();
    
    _cursor->setPointer(this->_text + len, false);
    
    this->toggleHint();
}

void CodeEditor::update(std::string str, bool updateString, bool updateCursor, size_t cursorStart, size_t cursorEnd) {

    // vxlog_info("UPATE FROM PLATFORM TEXT INPUT: cursorStart: %d, cursorEnd: %d", cursorStart, cursorEnd);

    if (updateString) {
        if (this->_text == nullptr) {
            this->_text_allocated = BUFFER_SIZE_MARGIN;
            this->_text = static_cast<char*>(malloc(sizeof(char) * this->_text_allocated));
            if (this->_text == nullptr) {
                return;
            }
        }

        this->increaseTextBufferToFitIfNecessary(str.size() + 1); // + 1 for null termination

        memcpy(this->_text, str.c_str(), str.size());
        this->_text[str.size()] = '\0';

        this->_refreshLineNumber();
        this->toggleHint();
    }

    if (updateCursor) {
        _cursor->setPointer(this->_text + cursorStart, false);
        if (cursorStart != cursorEnd) {
            _cursor->setPointer(this->_text + cursorEnd, true);
        }
        this->ensureCursorVisible();
    }
}

void CodeEditor::_ensureValidScrollY() {
    float max = getTotalHeight() - getViewHeight();
    if (_scrollY > max) { _scrollY = max; }
    if (_scrollY < 0.0f) { _scrollY = 0.0f; }
    _forceScrollY = true;
}

void CodeEditor::_refreshLineNumber() {
    const std::string str(this->_text);

    // count the number of lines
    _nbLines = std::count(str.begin(), str.end(), '\n') + 1;

    // vxlog_trace("NB LINES: %zu", _nbLines);

    int digits = 0;
    std::string sample; // placeholder for the width computation

    // compute common (base 10) logarithm (avoid log10(0) and log10(-1))
    digits = _nbLines > 0 ? static_cast<int>(floor(log10(_nbLines))) + 1 : 0; // +1 for upper bound

    sample = std::string(digits + 2, '9'); // +1 for \0 char

    // vxlog_trace("DIGITS: %d", digits);

    _lineGutterWidth =
        Font::shared()->calcImFontTextSize(this->_font, sample.c_str()).x * Screen::nbPixelsInOnePoint;

    // vxlog_trace("LINE DISPLAY WIDTH: %.2f", static_cast<double>(_lineGutterWidth));

    refreshLineParts();
    refreshSyntaxHighlight();
}

/// Gives the token group for each keyword and token available in Lua 5.3
/// Complete list can be found here: https://www.lua.org/manual/5.3/manual.html#3.1
CodeEditor::tokenGroup CodeEditor::_tokenTypeToTokenGroup(int type) {
    switch (type) {
        case Luau::Lexeme::Type::ReservedLocal:
        {
            return tokenGroup::local;
        }
        case Luau::Lexeme::Type::ReservedBreak:
        case Luau::Lexeme::Type::ReservedDo:
        case Luau::Lexeme::Type::ReservedElse:
        case Luau::Lexeme::Type::ReservedElseif:
        case Luau::Lexeme::Type::ReservedEnd:
        case Luau::Lexeme::Type::ReservedFor:
        case Luau::Lexeme::Type::ReservedFunction:
        case Luau::Lexeme::Type::ReservedIf:
        case Luau::Lexeme::Type::ReservedIn:
        case Luau::Lexeme::Type::ReservedRepeat:
        case Luau::Lexeme::Type::ReservedReturn:
        case Luau::Lexeme::Type::ReservedThen:
        case Luau::Lexeme::Type::ReservedUntil:
        case Luau::Lexeme::Type::ReservedWhile:
        {
            return tokenGroup::controlBlock;
        }
        case Luau::Lexeme::Type::ReservedNil:
        {
            return tokenGroup::nil;
        }
        case Luau::Lexeme::Type::ReservedTrue:
        case Luau::Lexeme::Type::ReservedFalse:
        {
            return tokenGroup::boolean;
        }
        case Luau::Lexeme::Type::Number:
        {
            return tokenGroup::number;
        }
        case Luau::Lexeme::Type::RawString:
        case Luau::Lexeme::Type::BrokenString:
        case Luau::Lexeme::Type::QuotedString:
        case Luau::Lexeme::Type::InterpStringEnd:
        case Luau::Lexeme::Type::InterpStringMid:
        case Luau::Lexeme::Type::InterpStringBegin:
        case Luau::Lexeme::Type::InterpStringSimple:
        case '\\':
        {
            return tokenGroup::string;
        }
        case Luau::Lexeme::Type::ReservedAnd:
        case Luau::Lexeme::Type::ReservedOr:
        case Luau::Lexeme::Type::ReservedNot:
        {
            return tokenGroup::binaryOp;
        }
        case '+':
        case '-':
        case '*':
        case '/':
        case Luau::Lexeme::Type::FloorDiv:
        case '%':
        case '^':
        {
            return tokenGroup::mathOp;
        }
        case '=':
        case Luau::Lexeme::Type::ConcatAssign:
        case Luau::Lexeme::Type::DivAssign:
        case Luau::Lexeme::Type::FloorDivAssign:
        case Luau::Lexeme::Type::MulAssign:
        case Luau::Lexeme::Type::AddAssign:
        case Luau::Lexeme::Type::SubAssign:
        case Luau::Lexeme::Type::ModAssign:
        case Luau::Lexeme::Type::PowAssign:
        {
            return tokenGroup::assignationOp;
        }
        case '#':
        {
            return tokenGroup::lengthOp;
        }
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case ',': {
            return tokenGroup::brackets;
        }
        case Luau::Lexeme::Type::Equal:
        case Luau::Lexeme::Type::NotEqual:
        case Luau::Lexeme::Type::LessEqual:
        case Luau::Lexeme::Type::GreaterEqual:
        case '>':
        case '<':
        {
            return tokenGroup::comparisonOp;
        }
        case Luau::Lexeme::Type::Comment:
        case Luau::Lexeme::Type::BlockComment:
        {
            return tokenGroup::comment;
        }
        default:
        {
            return tokenGroup::other;
        }
    }
}

/// Returns a copy of the CodeEditor's text
std::string CodeEditor::getText() const {
#ifndef P3S_CLIENT_HEADLESS
    return std::string(this->_text, strlen(this->_text));
#else
    return "";
#endif
}

bool CodeEditor::hasSelection() {
    return this->_cursor->hasSelection();
}

void CodeEditor::refreshSelectedIssue() {
//    if (_luaCheckReport.size() > 0) {
//        int32_t line = _cursor->getStartLine() + 1;
//
//        if (_parsedLines.empty() == false) {
//            for (ParsedLine l : _parsedLines) {
//                if (l.isFirstPart) {
//                    if (line <= static_cast<int32_t>(l.lineNo)) {
//                        break;
//                    }
//                } else {
//                    line -= 1;
//                }
//            }
//        }
//
//        Linter::LuaCheckReport::iterator reportIt = _luaCheckReport.find(line);
//        if (reportIt != _luaCheckReport.end()) {
//            if (_onSelectIssuesCallback) {
//                _onSelectIssuesCallback(reportIt->second);
//            }
//        } else {
//            if (_onUnselectIssuesCallback != nullptr) {
//                _onUnselectIssuesCallback();
//            }
//        }
//    } else {
//        if (_onUnselectIssuesCallback != nullptr) {
//            _onUnselectIssuesCallback();
//        }
//    }
}

void CodeEditor::ensureCursorVisible() {
    _cursorDT = 0.0;
    
    const float heightPx = (static_cast<float>(_height) - ((_padding + _borderSize) * 2.0f)) * Screen::nbPixelsInOnePoint;
    const int firstLine = static_cast<int>(floor(_scrollY * Screen::nbPixelsInOnePoint / _charAdvance.y));
    const int lastLine = std::max(0, std::min(static_cast<int>(_parsedLines.size()) - 1, firstLine + static_cast<int>(heightPx / _charAdvance.y)));
    
    if (this->_cursor->getEndLine() >= lastLine) {
        this->_scrollY = ((this->_cursor->getEndLine() + 1) * _charAdvance.y - heightPx) / Screen::nbPixelsInOnePoint;
    } else if (this->_cursor->getEndLine() <= firstLine) {
        this->_scrollY = this->_cursor->getEndLine() * _charAdvance.y / Screen::nbPixelsInOnePoint;
    }
    
    _ensureValidScrollY();

    refreshSelectedIssue();
}

void CodeEditor::increaseTextBufferToFitIfNecessary(const size_t wantedTotalSize) {
    if (wantedTotalSize > this->_text_allocated) {
        char* cStart = _cursor->getStart();
        size_t cStartOffset = cStart != nullptr ? cStart - this->_text : 0;
        char* cEnd = _cursor->getEnd();
        size_t cEndOffset = cEnd != nullptr ? cEnd - this->_text : 0;
        
        this->_text_allocated = wantedTotalSize + BUFFER_SIZE_MARGIN;
        this->_text = static_cast<char*>(realloc(this->_text, this->_text_allocated));
        
        if (cStart != nullptr && cEnd != nullptr) {
            _cursor->setPointer(this->_text + cStartOffset, false);
            _cursor->setPointer(this->_text + cEndOffset, true);
        }
    }
}

bool CodeEditor::isTextEmpty() {
    size_t len = strlen(_text);
    return _text != nullptr && (len == 0 || (len == 1 && _text[0] == '\n'));
}

void CodeEditor::refreshLineParts() {
    
#ifndef P3S_CLIENT_HEADLESS
    size_t textLen = strlen(this->_text);
    
    _parsedLines.clear();
    
    // optional line numbers
    const float currentLineGutterWidth = getLineGutterWidth();
    float lineWidth = this->_width * Screen::nbPixelsInOnePoint - currentLineGutterWidth - this->_padding * 2.0f * Screen::nbPixelsInOnePoint;
    
    // ParsedLine currentLine = ParsedLine(buf, false);
    char* start = this->_text;
    char* cursor = this->_text;
    const char* cut;
    bool isFirstPart = true;
    size_t lineNo = 1;

    // ⚠️ HERE: cleanup can't be done for the cursor, since parsedLines have cleared
    // Needs important changes in the way we place the cursor
    // char* cursorStart = this->_cursor->getStart();
    // char* cursorEnd = this->_cursor->getEnd();

    int charSize;

    // go through every character
    size_t textLenRemaining = textLen;
    bool exit = false;

    while (exit == false) {
        // move cursor until `\n` is found to add a new line
        // should also be executed when cursor reaches the end of the text
        if (textLenRemaining == 0 || *cursor == '\n') { // new line
            if (textLenRemaining == 0) {
                exit = true;
            }

            if (start == cursor) { // empty line (only \n)
                _parsedLines.push_back(ParsedLine(start, start, 0, true, lineNo));
            } else {
                // go through the line and split it into several lines if they are bigger than lineWidth
                while(start < cursor) {
                    size_t partLen = Font::shared()->getImFontFirstWarpPart(this->_font, start, cursor, &cut, lineWidth);
                    _parsedLines.push_back(ParsedLine(start, cut, partLen, isFirstPart, lineNo));
                    isFirstPart = false;
                    start += partLen;
                }
            }

            if (exit == false && *cursor == '\n') {
                // skip \n
                textLenRemaining--;
                cursor++;
                start++;
            }
            isFirstPart = true;
            lineNo++;

        } else { // didn't reach end of line, read next chars

            // move cursor forward
            charSize = utf8_char_size(cursor, textLenRemaining);
            if (charSize == 0) { // error
                exit = true;
            } else {
                if (textLenRemaining >= static_cast<size_t>(charSize)) {
                    textLenRemaining -= charSize;
                    cursor += charSize;
                } else {
                    cursor += textLenRemaining;
                    textLenRemaining = 0;
                }
            }
        }
    }
    
    // DEBUG
    //    char _buf[255];
    //    char _bufEscaped[512];
    //    size_t lineLen;
    //    size_t escapedLen = 0;
    //    for (ParsedLine line : _parsedLines) {
    //        vx_assert(line.start >= this->_text);
    //        lineLen = line.end - line.start;
    //        escapedLen = 0;
    //        snprintf(_buf, lineLen + 1, "%s", line.start);
    //
    //        for (size_t i = 0; i < lineLen; i++) {
    //            switch (_buf[i]) {
    //
    //                case '\t':
    //                    _bufEscaped[escapedLen++] = '\\';
    //                    _bufEscaped[escapedLen++] = 't';
    //                    break;
    //                case '\n':
    //                    _bufEscaped[escapedLen++] = '\\';
    //                    _bufEscaped[escapedLen++] = 'n';
    //                    break;
    //                case ' ':
    //                    // · char
    //                    _bufEscaped[escapedLen++] = 0xC2;
    //                    _bufEscaped[escapedLen++] = 0xB7;
    //                    break;
    //                default:
    //                    _bufEscaped[escapedLen++] = _buf[i];
    //            }
    //        }
    //        _bufEscaped[escapedLen] = '\0';
    //        vxlog_trace("L(%d): %s", line.length, _bufEscaped);
    //    }
    
    this->_cursor->setDirty();
#endif
}

/* chain list of long jump buffers */
struct lua_longjmp {
    struct lua_longjmp *previous;
    luai_jmpbuf b;
    volatile int status;  /* error code */
};

void CodeEditor::refreshSyntaxHighlight() {
    // TOKENS
    _tokens.clear();

    const size_t len = strlen(this->_text);

    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);

    Luau::Lexer lexer(this->_text, len, names);

    int startOffset = 0;
    int endOffset;

    Luau::Lexeme token = lexer.next();
    char *cursor = this->_text;

    while (token.type != Luau::Lexeme::Eof) {
        endOffset = lexer.getOffset();
        _tokens.push_back(Token(cursor + startOffset, cursor + endOffset, token.type));
        startOffset = lexer.getOffset();
        token = lexer.next();
    }

//    // REPORTS
//    _luaCheckReport.clear();
//    _codeCheckTimer = CODE_CHECK_DELAY;
//    refreshSelectedIssue();
//
}

void CodeEditor::refreshBackgroundColors() {
    this->setColor(_focused ? _focusedBgColor : _bgColor);
}

float CodeEditor::getLineGutterWidth() {
    // we keep a minimal line gutter width to be able to draw cursor rect on first character of a line
    return _displayLineNumbers ? _lineGutterWidth : 2.0f * Screen::nbPixelsInOnePoint;
}

#pragma mark CURSOR

CodeEditor::Cursor::Cursor(const CodeEditor_SharedPtr& codeEditor) :
_codeEditor(codeEditor),
_dirty(true),
_start(nullptr),
_end(nullptr),
_columnIsLocked(false),
_columnLockedToEndOfLine(false) {
    
}

CodeEditor::Cursor::~Cursor() {
    
}

// sets cursor pointer in raw text
void CodeEditor::Cursor::setPointer(char *p, bool selection) {
    
    CodeEditor_SharedPtr codeEditor = this->_codeEditor.lock();
    if (codeEditor == nullptr) return;
    
    if (p < codeEditor->_text || p > codeEditor->_text + strlen(codeEditor->_text)) {
        return;
    }

    if (selection == false) {
        _start = p;
        _end = _start;
        _selectionStart = _start;
    } else {
        if (p > _selectionStart) {
            _end = p;
            _start = _selectionStart;
        } else {
            _start = p;
            _end = _selectionStart;
        }
    }

    this->setDirty();
}

void CodeEditor::Cursor::setPosition(float x, float y, bool selection) {
    
    CodeEditor_SharedPtr codeEditor = this->_codeEditor.lock();
    if (codeEditor == nullptr) return;
    
    this->unlockColumn();

    char *cursor = codeEditor->pointToCursorPointer(x, y);
    if (cursor == nullptr) {
        return;
    }

    if (selection == false) {
        _start = cursor;
        _end = _start;
        _selectionStart = _start;
    } else {
        if (cursor > _selectionStart) {
            _end = cursor;
            _start = _selectionStart;
        } else { // cursor < _selectionStart
            _start = cursor;
            _end = _selectionStart;
        }
    }

    this->setDirty();
}

void CodeEditor::Cursor::setDirty() {
    this->_dirty = true;
}

void CodeEditor::Cursor::unlockColumn() {
    // invalidate values
    if (this->_columnIsLocked) {
        this->_columnIsLocked = false;
        this->_columnLockPosition = -1;
        this->_columnLockedToEndOfLine = false;
    }
}

void CodeEditor::Cursor::lockColumn() {

    CodeEditor_SharedPtr codeEditor = _codeEditor.lock();
    if (codeEditor == nullptr) {
        return;
    }

    if (_columnIsLocked) {
        // column is already locked
        return;
    }

    _columnIsLocked = true;

    // if there is a selection, we must focus on the end cursor
    int32_t line = this->getEndLine();
    const char *lineEnd = codeEditor->_parsedLines.at(line).end;

    _columnLockPosition = this->getEndColumn();
    if (_end == lineEnd) {
        _columnLockedToEndOfLine = true;
    }
}

bool CodeEditor::Cursor::hasSelection() {
    return this->_start != this->_end;
}

std::string CodeEditor::Cursor::getSelectionString() {
    
    if (this->hasSelection() == false) {
        return std::string("");
    }
    
    char *cStart = this->_start;
    char *cEnd = this->_end;
    
    // invert if end is before start
    if (cStart > cEnd) {
        char *tmp = cEnd;
        cEnd = cStart;
        cStart = tmp;
    }
    
    return std::string(cStart, cEnd - cStart);
}

void CodeEditor::Cursor::selectAll() {
    
    CodeEditor_SharedPtr codeEditor = this->_codeEditor.lock();
    if (codeEditor == nullptr) return;
    
    this->_start = codeEditor->_text;
    this->_end = codeEditor->_text + strlen(codeEditor->_text) - 1;
    this->setDirty();
}

void CodeEditor::Cursor::_clean() {
    
#ifndef P3S_CLIENT_HEADLESS
    if (this->_dirty == false) {
        // no need to clean
        return;
    }
    
    CodeEditor_SharedPtr codeEditor = this->_codeEditor.lock();
    if (codeEditor == nullptr) {
        return;
    }
    
    // set default values
    this->_startLine = 0;
    this->_startColumn = 0;
    this->_endLine = 0;
    this->_endColumn = 0;
    // NOTE: maybe we could avoid computing pixel position and do it dynamically
    // as we draw characters, counting for lines and columns.
    this->_startX = 0.0f;
    this->_startY = 0.0f;
    this->_endX = 0.0f;
    this->_endY = 0.0f;

    if (codeEditor->_parsedLines.size() == 0) {
        // no parsed lines, it's not possible to clean
        // return now, keeping dirty cursor.
        return;
    }
    
    unsigned int c;
    char *cursor;
    const char *lineEnd;
    
    // find lines
    
    bool foundEndLine = false;
    bool foundStartLine = false;
    
    for (size_t i = 0; i < codeEditor->_parsedLines.size(); i++) {
        
        cursor = codeEditor->_parsedLines.at(i).start;
        lineEnd = codeEditor->_parsedLines.at(i).end;

        if (foundStartLine == false && this->_start >= cursor && this->_start <= lineEnd) {
            this->_startLine = static_cast<int32_t>(i);
            foundStartLine = true;
            // vxlog_info("_clean found foundStartLine: %d - CURSOR on LINE: %d", this->_startLine, this->_start - codeEditor->_parsedLines.at(i).start);
        }
        
        if (foundEndLine == false && this->_end >= cursor && this->_end <= lineEnd) {
            this->_endLine = static_cast<int32_t>(i);
            foundEndLine = true;
        }

        if (foundStartLine && foundEndLine) {
            break;
        }
    }
    
    // error: could not find start line or end line
    if (foundStartLine == false || foundEndLine == false) {
        // return now, keeping dirty cursor.
        // vxlog_error("cursor: could not find start line or end line");
        return;
    }
    
    // find columns and define pixel positions
    
    // start cursor
    
    cursor = codeEditor->_parsedLines.at(this->_startLine).start;
    lineEnd = codeEditor->_parsedLines.at(this->_startLine).end;
    
    // vxlog_info("from line start - cursor: %d, lineEnd: %d", this->_start - cursor, lineEnd - cursor);

    while (cursor < this->_start && cursor < lineEnd) {
        c = static_cast<unsigned int>(*cursor);
        if (c < 0x80) {
            cursor += 1;
        } else {
            cursor += ImTextCharFromUtf8(&c, cursor, lineEnd);
            if (c == 0) // Malformed UTF-8?
                break;
        }
        this->_startX += Font::shared()->getImFontCharAdvanceX(codeEditor->_font, c);
        this->_startColumn++;
    }

    // vxlog_info("_startX: %.2f, column: %d", this->_startX, this->_startColumn);

    // end cursor
    
    cursor = codeEditor->_parsedLines.at(this->_endLine).start;
    lineEnd = codeEditor->_parsedLines.at(this->_endLine).end;
    
    while (cursor < this->_end && cursor < lineEnd) {
        c = static_cast<unsigned int>(*cursor);
        if (c < 0x80) {
            cursor += 1;
        } else {
            cursor += ImTextCharFromUtf8(&c, cursor, lineEnd);
            if (c == 0) // Malformed UTF-8?
                break;
        }
        this->_endX += Font::shared()->getImFontCharAdvanceX(codeEditor->_font, c);
        this->_endColumn++;
    }
    
    // update Y pixel positions
    this->_startY = this->_startLine * codeEditor->_charAdvance.y;
    this->_endY = this->_endLine * codeEditor->_charAdvance.y;
    
    // done cleaning
    this->_dirty = false;
#endif
}

int32_t CodeEditor::Cursor::getStartLine() {
    this->_clean();
    return this->_startLine;
}

int32_t CodeEditor::Cursor::getStartColumn() {
    this->_clean();
    return this->_startColumn;
}

int32_t CodeEditor::Cursor::getEndLine() {
    this->_clean();
    return this->_endLine;
}

int32_t CodeEditor::Cursor::getEndColumn() {
    this->_clean();
    return this->_endColumn;
}

float CodeEditor::Cursor::getStartX() {
    this->_clean();
    return this->_startX;
}

float CodeEditor::Cursor::getStartY() {
    this->_clean();
    return this->_startY;
}

float CodeEditor::Cursor::getEndX() {
    this->_clean();
    return this->_endX;
}

float CodeEditor::Cursor::getEndY() {
    this->_clean();
    return this->_endY;
}

char* CodeEditor::Cursor::getStart() {
    this->_clean();
    return this->_start;
}

char* CodeEditor::Cursor::getEnd() {
    this->_clean();
    return this->_end;
}
