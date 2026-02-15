//
//  VXVirtualKeyboardToolbar.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 03/07/2021.
//

#pragma once

#include "VXNode.hpp"
#include "VXButton.hpp"
#include "VXTickDispatcher.hpp"
#include "VXCodeEditor.hpp"

namespace vx {

class VirtualKeyboardToolbar;
typedef std::shared_ptr<VirtualKeyboardToolbar> VirtualKeyboardToolbar_SharedPtr;
typedef std::weak_ptr<VirtualKeyboardToolbar> VirtualKeyboardToolbar_WeakPtr;

class VirtualKeyboardToolbar final :
public Node,
public TickListener {

public:
    
    ///
    static VirtualKeyboardToolbar_SharedPtr make();

    ///
    ~VirtualKeyboardToolbar() override;
    
    void tick(const double dt) override;

private:
    
    ///
    VirtualKeyboardToolbar();
    
    ///
    void _init(const VirtualKeyboardToolbar_SharedPtr& ref);
    
    bool _cutAndCopyEnabled;
    bool _pasteEnabled;
    bool _undoEnabled;
    bool _redoEnabled;
    
    Button_SharedPtr _cutBtn;
    Button_SharedPtr _copyBtn;
    Button_SharedPtr _pasteBtn;
    Button_SharedPtr _undoBtn;
    Button_SharedPtr _redoBtn;
    
    void _refreshCutAndCopyBtns();
    void _refreshPasteBtn();
    void _refreshUndoRedoBtns();
    
    CodeEditor_SharedPtr _getFocusedCodeEditor();
    
    Button_SharedPtr _closeBtn;
};
}
