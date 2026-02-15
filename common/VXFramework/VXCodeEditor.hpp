//
//  VXCodeEditor.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/26/20.
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

#include <chrono>
#include <functional>

// vx
// #include "VXLinter.hpp"
#include "VXNode.hpp"
#include "VXNotifications.hpp"
#include "VXFont.hpp"
#include "inputs.h"

namespace vx {

class CodeEditor;
typedef std::shared_ptr<CodeEditor> CodeEditor_SharedPtr;
typedef std::weak_ptr<CodeEditor> CodeEditor_WeakPtr;

class CodeEditor final : public Node, public TickListener, public NotificationListener {
    
public:
    
    /**
     * Factory method
     */
    static CodeEditor_SharedPtr make(const std::string &str, const Font::Type& font = Font::Type::body);
    static CodeEditor_SharedPtr makeAsTextInput(const std::string &str, const std::string &hint="");
    
    /**
     * Destructor
     * This has to be "virtual" if class can be derived from.
     */
    ~CodeEditor() override;
    
    void render() override;
    
    bool press(const uint8_t &index, const float &x, const float &y) override;
    void longPress() override;
    void setLongPressPercentage(const float percentage, const float x, const float y) override;
    bool isLongPressSupported() override;
    void release(const uint8_t &index, const float &x, const float &y) override;
    void move(const float &x, const float &y, const float &dx, const float &dy, const uint8_t &index) override;
    void cancel(const uint8_t &index) override;
    
    bool focus() override;
    void unfocus() override;
    
    /// Sets text (does not trigger onTextChange)
    void setText(const std::string &str);

    ///
    void update(std::string str, bool updateString, bool updateCursor, size_t cursorStart, size_t cursorEnd);

    /// Sets callback to be triggered when text changes
    void setOnTextChange(std::function<void(const std::string&)> callback);
    
    /// Sets callback to be triggered when wrong content is entered
    void setOnTextError(std::function<void(const std::string&)> callback);

    ///
    void setOnScroll(std::function<void()> callback);

//    std::function<void(const Linter::LuaCheckIssues&)> _onSelectIssuesCallback;
//    std::function<void()> _onUnselectIssuesCallback;
//
//    ///
//    void setOnSelectIssues(std::function<void(const Linter::LuaCheckIssues&)> callback);
//    void setOnUnselectIssues(std::function<void()> callback);

    ///
    std::string getText() const;
    
    bool hasSelection();
    
    bool scroll(const float& dx, const float& dy) override;

    /// scrolls to the given percentage
    void goToStartPercentage(const float p);
    
    ///
    float getStartPercentage();
    
    ///
    float getViewPercentage();
    
    // Returns char* in _text for given coordinates
    char* pointToCursorPointer(float x, float y);
    
    /// Line numbers are displayed by default
    void toggleLineNumbers(bool);
    
    /// Syntax highlighting is enabled by default
    void toggleSyntaxHighlight(bool);
    void setNoHighlightTextColor(const Color&);
    
    /// Focused background color disabled by default
    void toggleFocusedColor(bool);
    void setBackgroundColor(const Color&);
    void setFocusedBackgroundColor(const Color&);
    
    /// Optional features disabled by default
    void setPasswordText(const std::string&);
    void setHintText(const std::string&);
    void setHintTextColor(const Color&);
    void setOneLine(bool value);
    void setMaxChar(const uint32_t value);
    void setRegex(std::string value);
    void setIsPassword(bool value);
    bool getIsPassword();

    float getTotalHeight();
    float getViewHeight();
    
    void triggerErrorEffect();
    
    inline void setReadOnly(bool b) { this->_readOnly = true; }
    inline bool getReadOnly() { return this->_readOnly; }
    inline void setAutoLower(bool b) { this->_autolower = b; }

    // saves current text & cursor position
    void saveState();
    // restores to last saved state if any
    void restoreState();
    
    void didReceiveNotification(const vx::Notification &notification) override;
    
    void handleInput(uint32_t charCode, Input input, const bool shift, const bool ctrl, const bool alt);
    
protected:
    
    /**
     * Default Constructor
     */
    CodeEditor();
    
    void init(const CodeEditor_SharedPtr& p,
              const std::string &str,
              const Font::Type &font);
    
    void toggleHint();
    
private:
    
    void _ensureValidScrollY();
    void _refreshLineNumber();

    enum class tokenGroup {
        local = 1,     // local keyword
        controlBlock,  // every keyword that uses end keyword
        jumps,         // goto keyword and ::labels::
        nil,           // nil keyword
        boolean,       // true false
        name,
        number,        // 42, 42.0, 4.2e1 0x0000002a
        string,        // all strings (' ', " ", [[ ]], ...) and concat (..)
        binaryOp,      // and or not keywords
        mathOp,        // + - * / // % ^
        bitwiseOp,     // & ~ | << >>
        comparisonOp,  // == ~= <= >= < >
        assignationOp, // =
        lengthOp,      // #
        brackets,      // ( ) [ ] { } ,
        comment,
        other,         // names ; ...
    };
    
    static CodeEditor::tokenGroup _tokenTypeToTokenGroup(int type);
    
    bool _focused;
    
    float _padding; // padding all around text
    float _borderSize; // added to the padding (border inside component)
    
    bool _isMovingCursor;
    
    float _longPressX;
    float _longPressY;
    
    float _moveX;
    float _moveY;
    
    bool _setPointerOnrelease;
    
    // optional features
    bool _displayLineNumbers;
    bool _useSyntaxHighlight;
    ImU32 _noHighlightColor;
    bool _useFocusColor;
    Color _bgColor, _focusedBgColor, _borderColor, _focusedBorderColor;
    std::string _hintText;
    std::string _passwordText;
    ImU32 _hintTextColor;
    bool _isHintTextActive;
    // auto lowers entered characters if true (false by default)
    bool _autolower;
    
    void onTextChange(const std::string& oldText);
    std::function<void(std::string)> _onTextChangeCallback;
    std::function<void(std::string)> _onTextErrorCallback;
    
    bool _oneLine;
    std::string _regex;
    uint32_t _maxChar;
    bool _isPassword;
    
    // how long it takes to fade out error effect
    float _errorEffectDuration;
    
    // 0.0 -> no error effect
    float _errorEffectLeft;
    
    void tick(const double dt) override;
    
    Font::Type _font;
    
    // text being displayed
    char *_text;
    size_t _text_allocated;
    
    // number of lines, based on line breaks (\n)
    size_t _nbLines;
    
    // gutter on the left to display line numbers
    float _lineGutterWidth;
    
    // current scroll Y position
    // float _scrollY; // already defined in (VXNode)
    
    // Tells imgui to set scroll to _scrollY when this is true
    bool _forceScrollY;
    
    //
    ImVec2 _charAdvance;
    
    typedef uint8_t Char;
    
    struct Line
    {
        std::string str;
        Line(const std::string& aStr) {
            str = aStr;
        }
    };

    struct ParsedLine
    {
        char* start;
        const char* end;
        size_t length;
        size_t lineNo;
        bool isFirstPart;
        char pad[7];
        
        ParsedLine(char* aStart, const char* aEnd, size_t aLength, bool aIsFirstPart, size_t aLineNo) :
        start(aStart),
        end(aEnd),
        length(aLength),
        lineNo(aLineNo),
        isFirstPart(aIsFirstPart) {}
    };
    
    struct Token {
        const char* start;
        const char* end;
        int type;
        char pad[4];
        
        Token(const char *aStart, const char *aEnd, int aType) :
        start(aStart),
        end(aEnd),
        type(aType) {}
    };

    // used to go back to a previous state
    std::string _previousText;
    float _previousScrollY;
    
    class Cursor
    {
    public:
        Cursor(const CodeEditor_SharedPtr& codeEditor);
        ~Cursor();

        // sets cursor pointer in raw text
        void setPointer(char *p, bool selection);
        
        // sets cursor position
        // x & y given in pixels
        void setPosition(float x, float y, bool selection);
        
        // enforces dirty cursor, called when recomputing lines for example.
        void setDirty();
        
        // start position in raw string
        char* getStart();
        
        // end position in raw string
        char* getEnd();

        // returns true when start != end
        bool hasSelection();
        
        // returns string built with selection
        std::string getSelectionString();

        // select the whole text w/o moving scroll
        void selectAll();
        
        // All following accessors will recompute values
        // when the cursor is dirty.
        
        // start line and column
        // IMPORTANT NOTE: line meaning "subline" here
        // A single line can be distributed on several
        // lines when area is not large enough
        int32_t getStartLine();
        int32_t getStartColumn();
        
        // end line and column
        // IMPORTANT NOTE: line meaning "subline" here
        // A single line can be distributed on several
        // lines when area is not large enough
        int32_t getEndLine();
        int32_t getEndColumn();
        
        // px positions for rendering,
        // where to draw the cursors.
        float getStartX();
        float getStartY();
        float getEndX();
        float getEndY();

    private:
        // Reference to the CodeEditor that instanciated the cursor.
        CodeEditor_WeakPtr _codeEditor;
        
        // Recomputes start line & column,
        // end line & column, and pixel positions.
        // (if dirty)
        void _clean();

        // used to go left / right
        void unlockColumn();
        // used to go up / down
        void lockColumn();
        
        // set to true when cursor moves
        bool _dirty;
        
        // start cursor position in raw text
        // _start == _end if there's no selection
        char* _start;
        // end cursor position in raw text
        char* _end;

        // where selection started
        char* _selectionStart;

        // Locked horizontal position for end cursor,
        // or both start & end when there's no selection.
        // Used to place the cursor at the right position when
        // moving with up/down arrows jumping to lines with
        // different lengths.
        bool _columnIsLocked;
        bool _columnLockedToEndOfLine;
        int32_t _columnLockPosition;
        
        // All following attributes will be recomputed
        // when the cursor is dirty.
        
        int32_t _startLine;
        int32_t _startColumn;
        
        int32_t _endLine;
        int32_t _endColumn;
        
        float _startX;
        float _endX;
        float _startY;
        float _endY;
    };
    
    std::vector<ParsedLine> _parsedLines;
    
    std::vector<Token> _tokens;

//    Linter::LuaCheckReport _luaCheckReport;
    // inserts text at cursor
    
    void increaseTextBufferToFitIfNecessary(const size_t wantedTotalSize);
    bool isTextEmpty();
    
    void refreshSelectedIssue();
    void ensureCursorVisible();
    
    void refreshLineParts();
    void refreshSyntaxHighlight();
    void refreshBackgroundColors();
    float getLineGutterWidth();
    
    Cursor *_cursor;
    double _cursorDT;

    double _codeCheckTimer;

    bool _readOnly;
    bool _cursorDirty;
    
    int _tabSize;
    
#ifndef P3S_CLIENT_HEADLESS
    /// variables required for rendering
    struct RenderVariables {
        float leftPx;
        float topPx;
        float widthPx;
        float widthWithoutPaddingAndBorderPx;
        float heightPx;
        float heightWithoutPaddingAndBorderPx;
        float parentHeightPx;
        float localLeft;
        float localTop;
        float paddingPx;
        float borderPx;
        float paddingAndBorderPx;
        float tmpFloat;
        unsigned int borderColor;
    };

    struct RenderVariables _renderVars;
#endif
};

}

#pragma clang diagnostic pop
