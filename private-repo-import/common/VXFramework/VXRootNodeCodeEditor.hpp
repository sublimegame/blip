//
//  VXMenuCodeEditor.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/26/20.
//

#pragma once

// vx
#include "VXNode.hpp"
#include "HttpRequest.hpp"
#include "VXCodeEditor.hpp"
#include "VXButton.hpp"
#include "VXScrollBar.hpp"

namespace vx {

class RootNodeCodeEditor;
typedef std::shared_ptr<RootNodeCodeEditor> RootNodeCodeEditor_SharedPtr;
typedef std::weak_ptr<RootNodeCodeEditor> RootNodeCodeEditor_WeakPtr;

class RootNodeCodeEditor final :
public Node,
public NotificationListener {

public:
    static RootNodeCodeEditor_SharedPtr make(const std::string& worldID, const std::string& script, bool readonly);

    ~RootNodeCodeEditor() override;

    // hides loading label and shows the code editor
    void hideLoading();
    void showLoading();
    
    void didReceiveNotification(const Notification& notification) override;

    const std::string& getWorldID();
    void updateScript(const std::string& script);
    void setReadOnly(bool readOnly);
    void setPublishLabelAndCallback(const std::string& label, std::function<void()> callback);
    void setCancelLabelAndCallback(const std::string& label, std::function<void()> callback);

private:
    
    RootNodeCodeEditor();
    
    ///
    void _init(const RootNodeCodeEditor_SharedPtr& ref);
    
    void didLayout() override;

    void setLayout(bool displayActions);

    CodeEditor_SharedPtr _codeEditor;

    ScrollBar_SharedPtr _scrollBar;
    
    Label_SharedPtr _loadingLabel;

    Button_SharedPtr _publishButton;
    Button_SharedPtr _cancelButton;
    Button_SharedPtr _docsButton;

    CodeEditor_SharedPtr _message;

    HttpRequest_SharedPtr _publishScriptReq;

    std::string _loadedWorldID;
    std::string _loadedScript;

    std::string _cancelLabel;
    std::function<void()> _cancelCallback;

    std::string _publishLabel;
    std::function<void()> _publishCallback;

};

}
