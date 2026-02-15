//
//  VXPopupEditText.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 11/01/2021.
//

#pragma once

#include "VXPopup.hpp"

// vx
#include "VXPopup.hpp"
#include "VXButton.hpp"
#include "VXCodeEditor.hpp"
#include "VXNotifications.hpp"

namespace vx {

    class PopupEditText;
    typedef std::shared_ptr<PopupEditText> PopupEditText_SharedPtr;
    typedef std::weak_ptr<PopupEditText> PopupEditText_WeakPtr;

    class PopupEditText final : public Popup {
    public:
        
        enum Mode {
            SingleLine,
            MultiLine,
        };
        
        typedef std::function<void(PopupEditText_SharedPtr popup, const std::string text)> DoneCallback;
        typedef std::function<void(PopupEditText_SharedPtr popup)> CancelCallback;
        
        static PopupEditText_SharedPtr makeMultiline(const std::string &title,
                                                     const std::string &hint,
                                                     const std::string &prefilledValue,
                                                     const std::string &regex = "",
                                                     const uint32_t maxChars = UINT32_MAX);
        
        /// Show loading overlay, blocking inputs
        void showLoading();
        
        /// Hide loading overlay
        void hideLoading();
        
        ///
        void setDoneCallback(DoneCallback c);
        void setCancelCallback(CancelCallback c);
        
        ///
        void setDoneLabel(const std::string& text);
        void setLoadingLabel(const std::string& text);

        /// Destructor
        ~PopupEditText() override;
        
    private:

        /// Constructor
        PopupEditText(Mode mode);
        
        void init(PopupEditText_SharedPtr p,
                  const std::string &title,
                  const std::string &hint,
                  const std::string &prefilledValue,
                  const std::string &regex,
                  const uint32_t maxChars);
                
        /// Variant of the popup
        Mode _mode;
                
        /// Title, describing the content to be typed
        Label_SharedPtr _titleLabel;
        
        /// Text input
        CodeEditor_SharedPtr _singlelineInput;
        CodeEditor_SharedPtr _multilineInput;
        
        /// Buttons
        Button_SharedPtr _btnDone;
        Button_SharedPtr _btnCancel;
        
        /// Loading overlay
        Node_SharedPtr _loading;
        Label_SharedPtr _loadingLabel;
        
        /// Callbacks
        DoneCallback _doneCallback;
        CancelCallback _cancelCallback;        

        ///
        void buildUIForMultiline(const std::string &title,
                                 const std::string &hint,
                                 const std::string &prefilledValue,
                                 const std::string &regex,
                                 const uint32_t maxChars);
        
        ///
        void cancelButtonCallback(Button_SharedPtr b, void *data);
        void doneButtonCallback(Button_SharedPtr b, void *data);
    };

}

