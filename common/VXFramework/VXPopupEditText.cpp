//
//  VXPopupEditText.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 11/01/2021.
//

#include "VXPopupEditText.hpp"

#include "GameCoordinator.hpp"

using namespace vx;

PopupEditText_SharedPtr PopupEditText::makeMultiline(const std::string &title,
                                                     const std::string &hint,
                                                     const std::string &prefilledValue,
                                                     const std::string &regex,
                                                     const uint32_t maxChars) {
    PopupEditText_SharedPtr popup(new PopupEditText(Mode::MultiLine));
    popup->init(popup, title, hint, prefilledValue, regex, maxChars);
    return popup;
}

void PopupEditText::showLoading() {
    _loading->setPassthrough(false);
    _loading->setVisible(true);
}

void PopupEditText::hideLoading() {
    _loading->setPassthrough(true);
    _loading->setVisible(false);
}

void PopupEditText::setDoneCallback(DoneCallback c) {
    this->_doneCallback = c;
}

void PopupEditText::setDoneLabel(const std::string& text) {
    this->_btnDone->setText(text.c_str(), Memory::Strategy::MAKE_COPY);
}

void PopupEditText::setLoadingLabel(const std::string& text) {
    this->_loadingLabel->setText(text.c_str(), Memory::Strategy::MAKE_COPY);
}

void PopupEditText::setCancelCallback(CancelCallback c) {
    this->_cancelCallback = c;
}

PopupEditText::PopupEditText(Mode mode) :
Popup(),
_mode(mode),
_titleLabel(nullptr),
_singlelineInput(nullptr),
_multilineInput(nullptr),
_btnDone(nullptr),
_btnCancel(nullptr),
_loading(nullptr),
_loadingLabel(nullptr),
_doneCallback(nullptr),
_cancelCallback(nullptr) {}

void PopupEditText::init(PopupEditText_SharedPtr p,
                         const std::string &title,
                         const std::string &hint,
                         const std::string &prefilledValue,
                         const std::string &regex,
                         const uint32_t maxChars) {
    Popup::init(p);
    
    switch (_mode) {
        case Mode::SingleLine:
            // NOT USED ANYMORE
            // this->buildUIForSingleline(title, hint, prefilledValue, regex, maxChars);
            break;
        case Mode::MultiLine:
            this->buildUIForMultiline(title, hint, prefilledValue, regex, maxChars);
            break;
    }
    
    // Loading overlay
    
    _loading = Node::make();
    _loading->setColor(MenuConfig::shared().colorTransparentBlack80);
    _loading->parentDidResize = [](Node_SharedPtr n){
        n->setSize(Screen::widthInPoints, Screen::heightInPoints);
        n->setPos(0.0f, Screen::heightInPoints);
    };
    this->addChild(_loading);
    
    _loadingLabel = Label::make("💾 Loading...",
                                Memory::Strategy::WEAK,
                                HorizontalAlignment::center,
                                Font::Type::title);
    _loadingLabel->setTextColor(MenuConfig::shared().colorWhite);
    _loading->parentDidResize = [](Node_SharedPtr n){
        n->setTop(Screen::heightInPoints * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(Screen::widthInPoints * 0.5f - n->getWidth() * 0.5f);
    };
    _loading->contentDidResize = [](Node_SharedPtr n){
        n->setTop(Screen::heightInPoints * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(Screen::widthInPoints * 0.5f - n->getWidth() * 0.5f);
    };
    _loading->addChild(_loadingLabel);
    _loading->setVisible(false);
}

PopupEditText::~PopupEditText() {}

void PopupEditText::buildUIForMultiline(const std::string &title,
                                        const std::string &hint,
                                        const std::string &prefilledValue,
                                        const std::string &regex,
                                        const uint32_t maxChars) {
    vx_assert(_mode == Mode::MultiLine);

    const float margin = MenuConfig::shared().marginSmall;
    
    Node_SharedPtr popupFrame = this->getFrame();
    Node_SharedPtr popupContainer = this->getContainer();
    
    popupFrame->setSizingPriority(SizingPriority::constraints, SizingPriority::constraints);
    popupContainer->setSizingPriority(SizingPriority::constraints, SizingPriority::constraints);
    
    // Title
    
    _titleLabel = Label::make(title.c_str(),
                              Memory::Strategy::MAKE_COPY,
                              HorizontalAlignment::center,
                              Font::Type::title);
    _titleLabel->setSizingPriority(SizingPriority::constraints, SizingPriority::constraints);
    _titleLabel->setTextColor(MenuConfig::shared().colorWhite);
    popupContainer->addChild(_titleLabel);
    
    // Text input
    
    _multilineInput = CodeEditor::makeAsTextInput(prefilledValue);
    _multilineInput->setExpectedKeyboard(KeyboardType::KeyboardTypeReturnOnReturn);
    _multilineInput->setRegex(regex);
    _multilineInput->setMaxChar(maxChars);
    popupContainer->addChild(_multilineInput);

    // Buttons

    _btnCancel = Button::makeWithText("Cancel",
                                      Memory::Strategy::WEAK,
                                      Font::Type::title);
    _btnCancel->setColors(MenuConfig::Theme::Negative);
    _btnCancel->setBeveled(true);
    _btnCancel->setSizingPriority(SizingPriority::constraints, SizingPriority::content);
    _btnCancel->onRelease([this](Button_SharedPtr b, void *data){
        this->cancelButtonCallback(b, data);
    }, nullptr);
    popupContainer->addChild(_btnCancel);

    _btnDone = Button::makeWithText("Done",
                                    Memory::Strategy::WEAK,
                                    Font::Type::title);
    _btnDone->setColors(MenuConfig::Theme::Positive);
    _btnDone->setBeveled(true);
    _btnDone->setSizingPriority(SizingPriority::constraints, SizingPriority::content);
    _btnDone->onRelease([this](Button_SharedPtr b, void *data){
        this->doneButtonCallback(b, data);
    }, nullptr);
    popupContainer->addChild(_btnDone);

    CodeEditor_WeakPtr multilineInputWeak = CodeEditor_WeakPtr(_multilineInput);
    Button_WeakPtr btnCancelWeak = Button_WeakPtr(_btnCancel);
    Button_WeakPtr btnDoneWeak = Button_WeakPtr(_btnDone);

    _titleLabel->parentDidResize = [margin, btnCancelWeak, btnDoneWeak, multilineInputWeak](Node_SharedPtr n) {
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }

        Button_SharedPtr btnCancel = btnCancelWeak.lock();
        if (btnCancel == nullptr) { return; }

        Button_SharedPtr btnDone = btnDoneWeak.lock();
        if (btnDone == nullptr) { return; }

        CodeEditor_SharedPtr multilineInput = multilineInputWeak.lock();
        if (multilineInput == nullptr) { return; }

        // title
        n->setWidth(parent->getWidth() - margin * 2.0f);
        n->setTop(parent->getHeight() - margin);
        n->setLeft(margin);

        // btnCancel
        btnCancel->setWidth(parent->getWidth() * 0.5f - margin * 1.5f);
        btnCancel->setTop(margin + btnCancel->getHeight());
        btnCancel->setLeft(margin);

        // btnDone
        btnDone->setWidth(parent->getWidth() * 0.5f - margin * 1.5f);
        btnDone->setTop(margin + btnDone->getHeight());
        btnDone->setLeft(parent->getWidth() * 0.5f + margin * 0.5f);

        // multilineInput
        multilineInput->setSize(parent->getWidth() - margin * 2,
                                parent->getHeight() - n->getHeight() - btnCancel->getHeight() - margin * 4);
        multilineInput->setTop(n->getTop() - n->getHeight() - margin);
        multilineInput->setLeft(margin);
    };
    _titleLabel->parentDidResizeWrapper();

    // This is buggy:
    // (so, no autofocus for now)
    //    if (_multilineInput->focus()) {
    //        Node::Manager::shared()->setFocused(_multilineInput);
    //    }
}

void PopupEditText::cancelButtonCallback(Button_SharedPtr b, void *data) {
    if (_cancelCallback != nullptr) {
        _cancelCallback(std::dynamic_pointer_cast<PopupEditText>(_weakSelf.lock()));
    } else {
        this->close();
    }
}

void PopupEditText::doneButtonCallback(Button_SharedPtr b, void *data) {
    std::string text;
    switch (_mode) {
        case Mode::SingleLine:
            text = _singlelineInput->getText();
            break;
        case Mode::MultiLine:
            text = _multilineInput->getText();
            break;
    }
    if (_doneCallback != nullptr) {
        _doneCallback(std::dynamic_pointer_cast<PopupEditText>(_weakSelf.lock()), text);
    } else {
        this->close();
    }
}
