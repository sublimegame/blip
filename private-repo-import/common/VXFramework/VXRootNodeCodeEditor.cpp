//
//  VXRootNodeCodeEditor.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/26/20.
//

#include "VXRootNodeCodeEditor.hpp"

// vx
#include "VXMenuConfig.hpp"
#include "VXHubClient.hpp"
#include "device.hpp"

#include "OperationQueue.hpp"

#include "GameCoordinator.hpp"

#include "xptools.h"

using namespace vx;

///
RootNodeCodeEditor_SharedPtr RootNodeCodeEditor::make(const std::string& worldID, const std::string& script, bool readonly) {
    RootNodeCodeEditor_SharedPtr p(new RootNodeCodeEditor);

    p->_loadedWorldID = worldID;
    p->_loadedScript = script;

    p->_init(p);

    if (readonly) {
        p->_publishButton->removeFromParent();
        p->_docsButton->removeFromParent();

        p->_cancelButton->setText("Close", Memory::Strategy::WEAK);
        p->_cancelButton->setColors(MenuConfig::shared().Negative);
        p->_cancelButton->setTextColor(MenuConfig::shared().colorWhite);

        p->_codeEditor->setReadOnly(true);

        ScrollBar_WeakPtr scrollBarWeak(p->_scrollBar);
        CodeEditor_WeakPtr codeEditorWeak = CodeEditor_WeakPtr(p->_codeEditor);

        p->setLayout(true);
    }
    return p;
}

void RootNodeCodeEditor::_init(const RootNodeCodeEditor_SharedPtr& ref) {
    
    Node::init(ref);
    
    this->setColor(MenuConfig::shared().colorTransparentBlack90);

    _publishButton = Button::makeWithText(_publishLabel.c_str(), Memory::Strategy::WEAK);
    _publishButton->setBeveled(true);
    _publishButton->setColors(MenuConfig::Theme::Positive);

    RootNodeCodeEditor_WeakPtr weakRef = RootNodeCodeEditor_WeakPtr(ref);
    _publishButton->onRelease([weakRef](Button_SharedPtr b, void *usrData) {
        RootNodeCodeEditor_SharedPtr self = weakRef.lock();
        if (self == nullptr) {
            return;
        }

        self->showLoading();
        const std::string newScript = self->_codeEditor->getText();

        if (self->_loadedWorldID != "") {
            self->_publishScriptReq = HubClient::getInstance().setWorldScript(HubClient::getInstance().getUniversalSender(),
                                                                        self->_loadedWorldID,
                                                                        newScript,
                                                                        [weakRef](const bool &success, HttpRequest_SharedPtr request){
                vx::OperationQueue::getMain()->dispatch([weakRef, success](){
                    RootNodeCodeEditor_SharedPtr self = weakRef.lock();
                    if (self == nullptr) {
                        return;
                    }
                    self->_publishScriptReq = nullptr;
                    self->hideLoading();

                    if (success) {
                        if (self->_publishCallback != nullptr) {
                            self->_publishCallback();
                        }
                    }
                });
            });
        }
    }, nullptr);

    this->addChild(_publishButton);
    
    _cancelButton = Button::makeWithText(_cancelLabel.c_str(), Memory::Strategy::WEAK);
    _cancelButton->setBeveled(true);
    _cancelButton->setColors(MenuConfig::Theme::Neutral);
    
    _cancelButton->onRelease([weakRef](Button_SharedPtr b, void *usrData) {
        RootNodeCodeEditor_SharedPtr self = weakRef.lock();
        if (self == nullptr) { return; }

        self->hideLoading();

        if (self->_publishScriptReq != nullptr) {
            self->_publishScriptReq->cancel();
            self->_publishScriptReq = nullptr;
        }

        // pressing "cancel" not while publishing, cancel changes and exit
        self->_codeEditor->restoreState();
        if (self->_cancelCallback != nullptr) {
            self->_cancelCallback();
        }
    }, nullptr);

    this->addChild(_cancelButton);
    
    this->_docsButton = Button::makeWithText("📘 Docs",
                                             Memory::Strategy::WEAK);
    _docsButton->setBeveled(true);
    _docsButton->setColorWithAutoHoverAndPressed(Color(static_cast<uint8_t>(110), static_cast<uint8_t>(105), static_cast<uint8_t>(106), static_cast<uint8_t>(255)));
    _docsButton->setTextColor(MenuConfig::shared().colorWhite);
    _docsButton->onRelease([](Button_SharedPtr b, void *usrData) {
        vx::Web::openModal("https://docs.cu.bzh/reference");
        // vx::Web::open("https://docs.cu.bzh/reference");
    }, nullptr);
    this->addChild(_docsButton);

    this->_message = CodeEditor::make("");
    this->_message->setReadOnly(true);
    this->_message->setOneLine(true);
    this->_message->toggleLineNumbers(false);
    this->_message->toggleFocusedColor(false);
    this->_message->toggleSyntaxHighlight(false);
    this->_message->setColor(MenuConfig::shared().colorBackgroundSuperDark);
    this->addChild(this->_message);

    CodeEditor_WeakPtr messageWeak(_message);

    const char* script = _loadedScript.c_str();
    
    _scrollBar = ScrollBar::make();
    this->addChild(_scrollBar);

    _codeEditor = CodeEditor::make(script);
    _codeEditor->setPassthrough(false);
    _codeEditor->setColor(MenuConfig::shared().colorTransparentBlack90);
    this->addChild(_codeEditor);

    CodeEditor_WeakPtr editorWeak(_codeEditor);
    _scrollBar->setOnMoveCallback([editorWeak](float startPercentage, void *userData) {
        CodeEditor_SharedPtr editor = editorWeak.lock();
        if (editor == nullptr) { return; }
        editor->goToStartPercentage(startPercentage);
    }, nullptr);

    ScrollBar_WeakPtr scrollBarWeak(_scrollBar);

    _codeEditor->setOnTextChange([scrollBarWeak, editorWeak](const std::string &newString) {
    ScrollBar_SharedPtr scrollBar = scrollBarWeak.lock();
        CodeEditor_SharedPtr editor = editorWeak.lock();
        if (scrollBar == nullptr || editor == nullptr) {
            return;
        }
        scrollBar->update(editor->getStartPercentage(), editor->getViewPercentage());
    });

//    _codeEditor->setOnUnselectIssues([editorWeak, messageWeak](){
//        CodeEditor_SharedPtr editor = editorWeak.lock();
//        CodeEditor_SharedPtr message = messageWeak.lock();
//        if (editor == nullptr) { return; }
//        if (message == nullptr) { return; }
//        message->setText("");
//        message->setNoHighlightTextColor(MenuConfig::shared().colorWhite);
//        message->setColor(MenuConfig::shared().colorBackgroundSuperDark);
//    });

//    _codeEditor->setOnSelectIssues([editorWeak, messageWeak](const Linter::LuaCheckIssues &issues){
//        CodeEditor_SharedPtr editor = editorWeak.lock();
//        CodeEditor_SharedPtr message = messageWeak.lock();
//        if (editor == nullptr) { return; }
//        if (message == nullptr) { return; }
//
//        if (issues.nErrors > 0) {
//            message->setText(issues.errors[0].message);
//            message->setNoHighlightTextColor(MenuConfig::shared().colorBackgroundSuperDark);
//            message->setColor(MenuConfig::shared().colorNegativeBackground);
//        } else if (issues.nWarnings > 0) {
//            message->setText(issues.warnings[0].message);
//            message->setNoHighlightTextColor(MenuConfig::shared().colorBackgroundSuperDark);
//            message->setColor(MenuConfig::shared().colorWarningBackground);
//        } else {
//            message->setText("");
//            message->setNoHighlightTextColor(MenuConfig::shared().colorWhite);
//            message->setColor(MenuConfig::shared().colorBackgroundSuperDark);
//        }
//    });

    _codeEditor->setOnScroll([scrollBarWeak, editorWeak]() {
        ScrollBar_SharedPtr scrollBar = scrollBarWeak.lock();
        CodeEditor_SharedPtr editor = editorWeak.lock();
        if (scrollBar == nullptr || editor == nullptr) {
            return;
        }
        scrollBar->update(editor->getStartPercentage(), editor->getViewPercentage());
    });
    
    _loadingLabel = Label::make("Saving...", Memory::Strategy::WEAK);
    _loadingLabel->setTextColor(MenuConfig::shared().colorWhite);
    _loadingLabel->setSizingPriority(Node::SizingPriority::content, Node::SizingPriority::content);
    _loadingLabel->parentDidResize = [](Node_SharedPtr n){
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }
        n->setPos(parent->getWidth() * 0.5f - n->getWidth() * 0.5f, parent->getHeight() * 0.5f + n->getHeight() * 0.5f);
    };
    _loadingLabel->setVisible(false);
    this->addChild(_loadingLabel);

    CodeEditor_WeakPtr codeEditorWeak = CodeEditor_WeakPtr(_codeEditor);
    Button_WeakPtr btnPublishWeak = Button_WeakPtr(_publishButton);
    Button_WeakPtr btnDocsWeak = Button_WeakPtr(_docsButton);

    setLayout(true);

    // notifications
    NotificationCenter::shared().addListener(this, NotificationName::showKeyboard);
    NotificationCenter::shared().addListener(this, NotificationName::hideKeyboard);
    NotificationCenter::shared().addListener(this, NotificationName::stateChanged);
}

// TODO: didResize
void RootNodeCodeEditor::didLayout() {
    if (_scrollBar != nullptr && _codeEditor != nullptr) {
        _scrollBar->update(_codeEditor->getStartPercentage(), _codeEditor->getViewPercentage());
    }
}

void RootNodeCodeEditor::setLayout(bool displayActions) {
    _cancelButton->parentDidResize = nullptr;
    _codeEditor->parentDidResize = nullptr;

    if (displayActions) {
        if (_codeEditor->getReadOnly() == false) {
            addChild(_docsButton);
            addChild(_publishButton);
            addChild(_message);
        }
        addChild(_cancelButton);

        ScrollBar_WeakPtr scrollBarWeak = ScrollBar_WeakPtr(_scrollBar);
        CodeEditor_WeakPtr codeEditorWeak = CodeEditor_WeakPtr(_codeEditor);
        Button_WeakPtr btnPublishWeak = Button_WeakPtr(_publishButton);
        Button_WeakPtr btnDocsWeak = Button_WeakPtr(_docsButton);
        CodeEditor_WeakPtr messageWeak = CodeEditor_WeakPtr(_message);

        _cancelButton->parentDidResize = [codeEditorWeak, scrollBarWeak, btnPublishWeak, btnDocsWeak, messageWeak](Node_SharedPtr n){
            Node_SharedPtr parent = n->getParent();
            if (parent == nullptr) { return; }

            CodeEditor_SharedPtr codeEditor = codeEditorWeak.lock();
            if (codeEditor == nullptr) { return; }

            CodeEditor_SharedPtr message = messageWeak.lock();
            if (message == nullptr) { return; }

            ScrollBar_SharedPtr scrollBar = scrollBarWeak.lock();
            if (scrollBar == nullptr) { return; }

            Node_SharedPtr btnCancel = n;

            Button_SharedPtr btnPublish = btnPublishWeak.lock();
            if (btnPublish == nullptr) { return; }

            Button_SharedPtr btnDocs = btnDocsWeak.lock();
            if (btnDocs == nullptr) { return; }

            float margin = MenuConfig::shared().marginSmall;

            if (codeEditor->getReadOnly()) {
                btnCancel->setTop(parent->getTop() - parent->getHeight() + btnCancel->getHeight() + margin + vx::Screen::safeAreaInsets.bottom);
                btnCancel->setLeft(parent->getWidth() * 0.5f - btnCancel->getWidth() * 0.5f);
            } else {
                btnCancel->setTop(parent->getTop() - parent->getHeight() + btnCancel->getHeight() + margin + vx::Screen::safeAreaInsets.bottom);
                btnCancel->setLeft(margin + vx::Screen::safeAreaInsets.left);

                btnDocs->setTop(parent->getTop() - parent->getHeight() + btnDocs->getHeight() + margin + vx::Screen::safeAreaInsets.bottom);
                btnDocs->setLeft(parent->getWidth() * 0.5f - btnDocs->getWidth() * 0.5f);

                btnPublish->setTop(parent->getTop() - parent->getHeight() + btnPublish->getHeight() + margin + vx::Screen::safeAreaInsets.bottom);
                btnPublish->setLeft(parent->getWidth() - btnPublish->getWidth() - margin - vx::Screen::safeAreaInsets.right);
            }

            message->setHeight(message->getTotalHeight());
            message->setTop(btnCancel->getTop() + message->getHeight() + margin);
            message->setLeft(margin);
            message->setWidth(parent->getWidth() - margin * 2);

            scrollBar->setHeight(parent->getHeight() - vx::Screen::safeAreaInsets.top - btnCancel->getHeight() - message->getHeight() - margin * 3);
            scrollBar->setTop(message->getTop() + scrollBar->getHeight());
            scrollBar->setLeft(parent->getWidth() - scrollBar->getWidth() - margin);

            codeEditor->setHeight(scrollBar->getHeight());
            codeEditor->setWidth(parent->getWidth() - margin * 2 - scrollBar->getWidth());
            codeEditor->setTop(scrollBar->getTop());
            codeEditor->setLeft(margin);
        };
        _cancelButton->parentDidResizeWrapper();

    } else {
        _docsButton->removeFromParent();
        _publishButton->removeFromParent();
        _cancelButton->removeFromParent();

        if (_codeEditor->getReadOnly() == false) {
            addChild(_message);
        } else {
            _message->removeFromParent();
        }

        ScrollBar_WeakPtr scrollBarWeak = ScrollBar_WeakPtr(_scrollBar);
        CodeEditor_WeakPtr messageWeak = CodeEditor_WeakPtr(_message);

        _codeEditor->parentDidResize = [scrollBarWeak, messageWeak](Node_SharedPtr n){
            Node_SharedPtr parent = n->getParent();
            if (parent == nullptr) { return; }

            Node_SharedPtr codeEditor = n;

            ScrollBar_SharedPtr scrollBar = scrollBarWeak.lock();
            if (scrollBar == nullptr) { return; }

            CodeEditor_SharedPtr message = messageWeak.lock();
            if (message == nullptr) { return; }

            float margin = MenuConfig::shared().marginSmall;

            message->setHeight(message->getTotalHeight());
            message->setTop(message->getHeight() + margin);
            message->setLeft(margin);
            message->setWidth(parent->getWidth() - margin * 2);

            scrollBar->setHeight(parent->getHeight() - vx::Screen::safeAreaInsets.top - message->getHeight() - margin * 2);
            scrollBar->setTop(scrollBar->getHeight() + message->getHeight() + margin);
            scrollBar->setLeft(parent->getWidth() - scrollBar->getWidth() - margin);

            codeEditor->setHeight(scrollBar->getHeight());
            codeEditor->setWidth(parent->getWidth() - margin * 2 - scrollBar->getWidth());
            codeEditor->setTop(scrollBar->getTop());
            codeEditor->setLeft(margin);
        };
        _codeEditor->parentDidResizeWrapper();
    }
}

RootNodeCodeEditor::RootNodeCodeEditor() :
Node(),
NotificationListener(),
_codeEditor(nullptr),
_publishScriptReq(nullptr),
_loadedWorldID(""),
_loadedScript(""),
_cancelLabel("Cancel"),
_cancelCallback(nullptr),
_publishLabel("Publish"),
_publishCallback(nullptr)
{}

RootNodeCodeEditor::~RootNodeCodeEditor() {}

void RootNodeCodeEditor::hideLoading() {
    _codeEditor->setVisible(true);
    _loadingLabel->setVisible(false);
    _publishButton->setHittable(true);
    _docsButton->setHittable(true);
}

void RootNodeCodeEditor::showLoading() {
    _codeEditor->setVisible(false);
    _loadingLabel->setVisible(true);
    _publishButton->setHittable(false);
    _docsButton->setHittable(false);
}

void RootNodeCodeEditor::didReceiveNotification(const Notification& notification) {
    switch (notification.name) {
        case NotificationName::showKeyboard:
        {
            // keyboardHeight == 0.0 here means hardware keyboard in use
            // do not show bottom bar in that case
            float keyboardHeight = GameCoordinator::getInstance()->keyboardSize().getHeight();
            if (keyboardHeight > 0.0f) {
                setLayout(false);
            } else { // hardware keyboard
                setLayout(true);
            }
            _scrollBar->update(_codeEditor->getStartPercentage(), _codeEditor->getViewPercentage());
            break;
        }
        case NotificationName::hideKeyboard:
        {
            setLayout(true);
            _scrollBar->update(_codeEditor->getStartPercentage(), _codeEditor->getViewPercentage());
            break;
        }
        case NotificationName::stateChanged:
        {
            State_SharedPtr state = GameCoordinator::getInstance()->getState();
            State::Value stateValue = state->getValue();
            if (stateValue == State::Value::activeWorldCodeReader ||
                stateValue == State::Value::activeWorldCodeEditor ||
                stateValue == State::Value::worldCodeEditor) {
                // Code editor is shown, let's save its state
                this->_codeEditor->saveState();
            }
        }
        default:
            break;
    }
}

const std::string& RootNodeCodeEditor::getWorldID() {
    return this->_loadedWorldID;
}

void RootNodeCodeEditor::updateScript(const std::string& script) {
    _codeEditor->update(script, true, true, 0, 0);
}

void RootNodeCodeEditor::setReadOnly(bool readOnly) {
    // TODO: implement
}

void RootNodeCodeEditor::setPublishLabelAndCallback(const std::string& label, std::function<void()> callback) {
    _publishLabel = label;
    _publishCallback = callback;
    _publishButton->setText(_publishLabel.c_str(), Memory::Strategy::WEAK);
}

void RootNodeCodeEditor::setCancelLabelAndCallback(const std::string& label, std::function<void()> callback) {
    _cancelLabel = label;
    _cancelCallback = callback;
    _cancelButton->setText(_cancelLabel.c_str(), Memory::Strategy::WEAK);
}
