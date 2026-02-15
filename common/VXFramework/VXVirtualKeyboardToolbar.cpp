//
//  VXVirtualKeyboardToolbar.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 03/07/2021.
//

#include "VXVirtualKeyboardToolbar.hpp"
#include "xptools.h"
#include "GameCoordinator.hpp"

#include <cmath>

#define TOOLBAR_MARGIN MenuConfig::shared().marginVerySmall
#define OPACITY_DISABLED 0.3f

using namespace vx;

VirtualKeyboardToolbar_SharedPtr VirtualKeyboardToolbar::make() {
    VirtualKeyboardToolbar_SharedPtr toolBar(new VirtualKeyboardToolbar());
    toolBar->_init(toolBar);
    return toolBar;
}

VirtualKeyboardToolbar::~VirtualKeyboardToolbar() {}

VirtualKeyboardToolbar::VirtualKeyboardToolbar() :
Node(),
_cutAndCopyEnabled(false),
_pasteEnabled(true), // always enabled
_undoEnabled(false),
_redoEnabled(false),
_cutBtn(nullptr),
_copyBtn(nullptr),
_pasteBtn(nullptr),
_undoBtn(nullptr),
_redoBtn(nullptr),
_closeBtn(nullptr) {}

void VirtualKeyboardToolbar::_init(const VirtualKeyboardToolbar_SharedPtr& ref) {
    
    this->_weakSelf = ref;
    this->setPassthrough(false);
    this->setUnfocusesFocusedNode(false);
    this->setSizingPriority(SizingPriority::constraints, SizingPriority::content);
    this->setColor(Color(uint8_t(63), uint8_t(63), uint8_t(63), uint8_t(255)));
    
    this->_cutBtn = Button::makeWithText("✂️", Memory::Strategy::WEAK,
                                         Font::Type::body);
    this->_cutBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    this->_cutBtn->setBeveled(true);
    this->_cutBtn->onRelease([](const Button_SharedPtr &btn, void *userdata){
        
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused == nullptr) { return; }
        CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
        if (codeEditor == nullptr) { return; }
        vx::textinput::textInputAction(TextInputAction_Cut);

    }, nullptr);
    this->addChild(_cutBtn);
    
    this->_copyBtn = Button::makeWithText("📑", Memory::Strategy::WEAK,
                                          Font::Type::body);
    this->_copyBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    this->_copyBtn->setBeveled(true);
    this->_copyBtn->onRelease([](const Button_SharedPtr &btn, void *userdata){
        
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused == nullptr) { return; }
        CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
        if (codeEditor == nullptr) { return; }
        vx::textinput::textInputAction(TextInputAction_Copy);
    }, nullptr);

    this->addChild(_copyBtn);
    
    this->_pasteBtn = Button::makeWithText("📋", Memory::Strategy::WEAK,
                                           Font::Type::body);
    this->_pasteBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    this->_pasteBtn->setBeveled(true);
    this->_pasteBtn->onRelease([](const Button_SharedPtr &btn, void *userdata){
        
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused == nullptr) { return; }
        CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
        if (codeEditor == nullptr) { return; }
        vx::textinput::textInputAction(TextInputAction_Paste);

    }, nullptr);
    this->addChild(_pasteBtn);

    _undoBtn = Button::makeWithText("↪️", Memory::Strategy::WEAK, Font::Type::body);
    _undoBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    _undoBtn->setBeveled(true);
    _undoBtn->onRelease([](const Button_SharedPtr &btn, void *userdata) {
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused == nullptr) {
            return;
        }
        CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
        if (codeEditor == nullptr) {
            return;
        }
        vx::textinput::textInputAction(TextInputAction_Undo);
    }, nullptr);
    this->addChild(_undoBtn);

    _redoBtn = Button::makeWithText("↩️", Memory::Strategy::WEAK, Font::Type::body);
    _redoBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    _redoBtn->setBeveled(true);
    _redoBtn->onRelease([](const Button_SharedPtr &btn, void *userdata) {
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused == nullptr) {
            return;
        }
        CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
        if (codeEditor == nullptr) {
            return;
        }
        vx::textinput::textInputAction(TextInputAction_Redo);
    }, nullptr);
    this->addChild(_redoBtn);

    // CLOSE BUTTON
    this->_closeBtn = Button::makeWithText("⬇️", Memory::Strategy::WEAK,
                                           Font::Type::body);
    this->_closeBtn->setColor(MenuConfig::shared().colorBackgroundDark);
    this->_closeBtn->setBeveled(true);
    this->_closeBtn->onRelease([](const Button_SharedPtr &btn, void *userdata){
        
        Node::Manager::shared()->loseFocus();
        Game_SharedPtr game = GameCoordinator::getInstance()->getActiveGame();
        if (game != nullptr) {
            game->setActiveKeyboard(KeyboardTypeNone);
        }
        
    }, nullptr);
    this->addChild(_closeBtn);

    Button_WeakPtr cutBtnWeak = Button_WeakPtr(_cutBtn);
    Button_WeakPtr copyBtnWeak = Button_WeakPtr(_copyBtn);
    Button_WeakPtr pasteBtnWeak = Button_WeakPtr(_pasteBtn);
    Button_WeakPtr closeBtnWeak = Button_WeakPtr(_closeBtn);

    this->parentDidResizeSystem = [cutBtnWeak, copyBtnWeak, pasteBtnWeak, closeBtnWeak](Node_SharedPtr n){
        Button_SharedPtr cutBtn = cutBtnWeak.lock();
        Button_SharedPtr copyBtn = copyBtnWeak.lock();
        Button_SharedPtr pasteBtn = pasteBtnWeak.lock();
        Button_SharedPtr closeBtn = closeBtnWeak.lock();

        if (cutBtn == nullptr || copyBtn == nullptr || pasteBtn == nullptr || closeBtn == nullptr) {
            return;
        }

        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }

        n->setSize(parent->getWidth(), cutBtn->getHeight() + TOOLBAR_MARGIN * 2);

        cutBtn->setPos(TOOLBAR_MARGIN, cutBtn->getHeight() + TOOLBAR_MARGIN);
        copyBtn->setPos(cutBtn->getLeft() + cutBtn->getWidth() + TOOLBAR_MARGIN, cutBtn->getTop());
        pasteBtn->setPos(copyBtn->getLeft() + copyBtn->getWidth() + TOOLBAR_MARGIN, copyBtn->getTop());
        closeBtn->setPos(n->getWidth() - closeBtn->getWidth() - TOOLBAR_MARGIN, copyBtn->getTop());
    };
    this->parentDidResizeWrapper();

    this->_refreshCutAndCopyBtns();
    this->_refreshPasteBtn();
    this->_refreshUndoRedoBtns();
}

// NOTE: aduermael: it would be better to use events instead of
// pulling the information within a tick loop.
// But it was faster to implement this way (for 0.0.24)
// Especially because clipboard events require platform specific implementations.
void VirtualKeyboardToolbar::tick(const double dt) {
      
    CodeEditor_SharedPtr codeEditor = _getFocusedCodeEditor();
    if (codeEditor == nullptr) {
        return;
    }
    
    if (_cutAndCopyEnabled == false && codeEditor->hasSelection()) {
        _cutAndCopyEnabled = true;
        _refreshCutAndCopyBtns();
    } else if (_cutAndCopyEnabled && codeEditor->hasSelection() == false) {
        _cutAndCopyEnabled = false;
        _refreshCutAndCopyBtns();
    }

    // TODO: NOTIFICATION FOR HOST PLATFORM TEXT INPUT
    //    if (_undoEnabled != codeEditor->canUndo() || _redoEnabled != codeEditor->canRedo()) {
    //        _undoEnabled = codeEditor->canUndo();
    //        _redoEnabled = codeEditor->canRedo();
    //        this->_refreshUndoRedoBtns();
    //    }
}

CodeEditor_SharedPtr VirtualKeyboardToolbar::_getFocusedCodeEditor() {
    
    Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
    Node_SharedPtr focused = focusedWeak.lock();
    if (focused == nullptr) {
        // if nothing focused, toolbar should not even be displayed
        return nullptr;
    }
    CodeEditor_SharedPtr codeEditor = std::dynamic_pointer_cast<CodeEditor>(focused);
    if (codeEditor == nullptr) {
        return nullptr;
    }
    
    return codeEditor;
}

void VirtualKeyboardToolbar::_refreshCutAndCopyBtns() {
    if (_cutAndCopyEnabled) {
        _cutBtn->setOpacity(1.0f);
        _cutBtn->setHittable(true);
        _copyBtn->setOpacity(1.0f);
        _copyBtn->setHittable(true);
    } else {
        _cutBtn->setOpacity(OPACITY_DISABLED);
        _cutBtn->setHittable(false);
        _copyBtn->setOpacity(OPACITY_DISABLED);
        _copyBtn->setHittable(false);
    }
}

void VirtualKeyboardToolbar::_refreshPasteBtn() {
    if (_pasteEnabled) {
        _pasteBtn->setOpacity(1.0f);
        _pasteBtn->setHittable(true);
    } else {
        _pasteBtn->setOpacity(OPACITY_DISABLED);
        _pasteBtn->setHittable(false);
    }
}

void VirtualKeyboardToolbar::_refreshUndoRedoBtns() {
    if (_undoEnabled) {
        _undoBtn->setOpacity(1.0f);
        _undoBtn->setHittable(true);
    } else {
        _undoBtn->setOpacity(OPACITY_DISABLED);
        _undoBtn->setHittable(false);
    }

    if (_redoEnabled) {
        _redoBtn->setOpacity(1.0f);
        _redoBtn->setHittable(true);
    } else {
        _redoBtn->setOpacity(OPACITY_DISABLED);
        _redoBtn->setHittable(false);
    }
}
