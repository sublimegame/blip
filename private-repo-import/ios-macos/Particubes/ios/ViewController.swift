//
//  ViewController.swift
//  ios
//
//  Created by Adrien Duermael on 2/13/20.
//

import UIKit
import MetalKit

class ViewController: UIViewController {
    static var currentInstance: ViewController?

    private let textField = CustomTextField()
    private let textView = CustomTextView() // multiline

    private var tickTimer: Timer?
    private var lastTick: DispatchTime?
    
    private var m_device: MTLDevice?

    // static    id<MTLDevice>  m_device = NULL;

    // Swift representation of the C callbacks
    typealias HostPlatormTextInput_RequestCallback = @convention(c) (UnsafePointer<CChar>?, Int, Bool, Int, Int, Bool, TextInputKeyboardType, TextInputReturnKeyType, Bool) -> Void
    typealias NTID_UpdateCallback = @convention(c) (UnsafePointer<CChar>?, Int, Bool, Int, Int) -> Void
    typealias HostPlatormTextInput_ActionCallback = @convention(c) (TextInputAction) -> Void

    var textinputRequestCallbackPtr: HostPlatormTextInput_RequestCallback? = nil
    var textinputUpdateCallbackPtr: NTID_UpdateCallback? = nil
    var textinputActionCallbackPtr: HostPlatormTextInput_ActionCallback? = nil

    static func textInputRequestCallback(data: Data,
                                         strDidChange: Bool,
                                         cursorStart: Int,
                                         cursorEnd: Int,
                                         multiline: Bool,
                                         keyboardType:
                                         TextInputKeyboardType,
                                         returnKeyType: TextInputReturnKeyType,
                                         suggestions: Bool) {

        guard let self = ViewController.currentInstance else { return }

        if multiline {
            DispatchQueue.main.async {
                // hide predictive text bar (more room)
                self.textView.autocorrectionType = .no
                self.textView.spellCheckingType = .no
                self.textView.smartDashesType = .no
                self.textView.smartQuotesType = .no
                self.textView.smartInsertDeleteType = .default
                // NOTE: could be exposed to Lua
                // (no auto capitalization for scripts, but auto capitalization for texts)
                // self.textField.autocapitalizationType

                self.textView.autocapitalizationType = .none

                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                if strDidChange {
                    self.textView.isDidChangeTurnedOff = true
                    self.textView.text = s.string
                    self.textView.isDidChangeTurnedOff = false
                }

                self.textView.becomeFirstResponder()

                self.textView.isDidChangeTurnedOff = true
                _ = setGraphemeClusterAwareRange(textView: self.textView, string: s.string, start: s.start, end: s.end)
                self.textView.isDidChangeTurnedOff = false
            }
        } else {
            self.textField.returnKeyType = .default
            DispatchQueue.main.async {

                switch returnKeyType {
                case TextInputReturnKeyType_Default:
                    self.textField.returnKeyType = .default
                case TextInputReturnKeyType_Next:
                    self.textField.returnKeyType = .next
                case TextInputReturnKeyType_Done:
                    self.textField.returnKeyType = .done
                case TextInputReturnKeyType_Send:
                    self.textField.returnKeyType = .send
                default:
                    self.textField.returnKeyType = .default
                }

                switch keyboardType {
                case TextInputKeyboardType_Phone:
                    // NOTE: no need to ask for suggestions when content type is .telephoneNumber
                    // main phone number suggested automatically, and we don't want other suggestions
                    self.textField.keyboardType = .phonePad
                    self.textField.textContentType = .telephoneNumber
                case TextInputKeyboardType_OneTimeDigicode:
                    self.textField.keyboardType = .phonePad
                    self.textField.textContentType = .oneTimeCode
                default:
                    self.textField.keyboardType = .default
                    self.textField.textContentType = .none
                }

                // hide predictive text bar (more room)
                if (suggestions) {
                    self.textField.autocorrectionType = .yes
                    self.textField.spellCheckingType = .yes
                    self.textField.smartDashesType = .no
                    self.textField.smartQuotesType = .no
                    self.textField.smartInsertDeleteType = .default
                } else {
                    self.textField.autocorrectionType = .no
                    self.textField.spellCheckingType = .no
                    self.textField.smartDashesType = .no
                    self.textField.smartQuotesType = .no
                    self.textField.smartInsertDeleteType = .default
                }

                self.textField.autocapitalizationType = .none

                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                if strDidChange {
                    self.textField.isDidChangeTurnedOff = true
                    self.textField.text = s.string
                    self.textField.isDidChangeTurnedOff = false
                }

                self.textField.becomeFirstResponder()

                self.textField.isDidChangeTurnedOff = true
                _ = setTextFieldGraphemeClusterAwareRange(textField: self.textField, string: s.string, start: s.start, end: s.end)
                self.textField.isDidChangeTurnedOff = false
            }
        }
    }

    static func textInputUpdateCallback(data: Data,
                                        strDidChange: Bool,
                                        cursorStart: Int,
                                        cursorEnd: Int) {

        guard let self = ViewController.currentInstance else { return }


        DispatchQueue.main.async {
            if self.textView.isFirstResponder {
                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                self.textView.isDidChangeTurnedOff = true
                if strDidChange {
                    self.textView.text = s.string
                }
                _ = setGraphemeClusterAwareRange(textView: self.textView, string: s.string, start: s.start, end: s.end)
                self.textView.isDidChangeTurnedOff = false

            } else if self.textField.isFirstResponder {
                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                self.textField.isDidChangeTurnedOff = true
                if strDidChange {
                    self.textField.text = s.string
                }
                _ = setTextFieldGraphemeClusterAwareRange(textField: self.textField, string: s.string, start: s.start, end: s.end)
                self.textField.isDidChangeTurnedOff = false
            }
        }
    }

    func _selectAll() {
        if textView.isFirstResponder {
            textView.selectAll(nil)
        } else if textField.isFirstResponder {
            textField.selectAll(nil)
        }
    }

    func _copy() {
        if textView.isFirstResponder {
            textView.copy(nil)
        } else if textField.isFirstResponder {
            textField.copy(nil)
        }
    }

    func _paste() {
        if textView.isFirstResponder {
            textView.paste(nil)
        } else if textField.isFirstResponder {
            textField.paste(nil)
        }
    }

    func _cut() {
        if textView.isFirstResponder {
            textView.cut(nil)
        } else if textField.isFirstResponder {
            textField.cut(nil)
        }
    }

    func _undo() {
        if textView.isFirstResponder {
            textView.undoManager?.undo()
        } else if textField.isFirstResponder {
            textField.undoManager?.undo()
        }
    }

    func _redo() {
        if textView.isFirstResponder {
            textView.undoManager?.redo()
        } else if textField.isFirstResponder {
            textField.undoManager?.redo()
        }
    }

    static func textInputActionCallback(action: TextInputAction) {
        guard let self = ViewController.currentInstance else { return }
        DispatchQueue.main.async {
            switch action {
            case TextInputAction_Copy:
                self._copy()
            case TextInputAction_Paste:
                self._paste()
            case TextInputAction_Cut:
                self._cut()
            case TextInputAction_Undo:
                self._undo()
            case TextInputAction_Redo:
                self._redo()
            case TextInputAction_Close:
                self.textField.endEditing(true)
                self.textView.endEditing(true)
            default:
                return
            }
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        ViewController.currentInstance = self

        textField.delegate = self
        self.view.addSubview(textField)
        textField.isHidden = true

        textView.delegate = self
        self.view.addSubview(textView)
        textView.isHidden = true

        self.view.isMultipleTouchEnabled = true

        guard let mtkView = self.view as? MTKView else {
            print("🔥 window view should be an MTKView");
            return
        }

        // from : https://stackoverflow.com/questions/44684031/swift-3-how-to-supply-a-c-style-void-object-to-ios-frameworks-for-use-with
        let viewUnmanaged: Unmanaged = Unmanaged.passRetained(mtkView)
        let viewPtr: UnsafeMutableRawPointer = viewUnmanaged.toOpaque()

        // let layerUnmanaged: Unmanaged = Unmanaged.passRetained(self.view!.layer)
        // let layerPtr: UnsafeMutableRawPointer = layerUnmanaged.toOpaque()

        let contentRect = mtkView.frame

        m_device = MTLCreateSystemDefaultDevice()

        mtkView.contentScaleFactor = UIScreen.main.scale

        let deviceUnmanaged: Unmanaged = Unmanaged.passRetained(m_device!)
        let devicePtr: UnsafeMutableRawPointer = deviceUnmanaged.toOpaque()

        let sai = mtkView.safeAreaInsets
        let scale = Float(mtkView.contentScaleFactor)

        var width = UInt32(contentRect.width * CGFloat(scale))
        var height = UInt32(contentRect.height * CGFloat(scale))

        // NOTE: Ugly fix for iPhone Pro Max
        // For some reason, render pass height ends up being set to 2435
        // instead of 2436. The app crashes when validating MLL scissor rect.
        // This will certainly be fixed with a bgfx update.
        // In the meantime, this does the job.
        // (fix also used in viewDidLayoutSubviews)
        if height == 2436 {
            height = 2435
        } else if width == 2436 {
            width = 2435
        }

        vxapplication_didFinishLaunching(viewPtr,
                                         devicePtr,
                                         width,
                                         height,
                                         scale,
                                         vx_insets(top: Float(sai.top),
                                                   bottom: Float(sai.bottom),
                                                   left: Float(sai.left),
                                                   right: Float(sai.right)),
                                         "");

        let updater = CADisplayLink(target: self, selector: #selector(tick))
        updater.preferredFramesPerSecond = 50 // 60
        updater.add(to: .current, forMode: .default)


        textinputRequestCallbackPtr = {(str: UnsafePointer<CChar>?, strLen: Int, strDidChange: Bool, cursorStart: Int, cursorEnd: Int, multiline: Bool, keyboardType: TextInputKeyboardType, returnKeyType: TextInputReturnKeyType, suggestions: Bool) -> Void in

            guard let safeStr = str else {
                return
            }
            let data = Data(bytes: safeStr, count: strLen)

            ViewController.textInputRequestCallback(data: data, strDidChange: strDidChange, cursorStart: cursorStart, cursorEnd: cursorEnd, multiline: multiline, keyboardType: keyboardType, returnKeyType: returnKeyType, suggestions: suggestions)
        }

        textinputUpdateCallbackPtr = {(str: UnsafePointer<CChar>?, strLen: Int, strDidChange: Bool, cursorStart: Int, cursorEnd: Int) -> Void in

            guard let safeStr = str else {
                return
            }
            let data = Data(bytes: safeStr, count: strLen)

            ViewController.textInputUpdateCallback(data: data, strDidChange: strDidChange, cursorStart: cursorStart, cursorEnd: cursorEnd)
        }

        textinputActionCallbackPtr = { (action: TextInputAction) in
            ViewController.textInputActionCallback(action: action)
        }

        hostPlatformTextInputRegisterDelegate(textinputRequestCallbackPtr,
                                              textinputUpdateCallbackPtr,
                                              textinputActionCallbackPtr);
    }
    
    @objc func tick(sender: CADisplayLink) {
        // sender.duration contains duration since last screen refresh it seems
        // even if ticks have been skipped.
        // That's why have to use DispatchTime.now() and compute the diff
        // let dt: Double = min(sender.duration, Double(0.1)) // 0.1 -> 10 FPS
        // vxapplication_tick(dt)
        
        let now = DispatchTime.now()
        
        if let lastTick = self.lastTick {
            let nanoTime = now.uptimeNanoseconds - lastTick.uptimeNanoseconds
            var dt : Double = Double(nanoTime) / Double(1_000_000_000) // in seconds
            // Simulation happens at normal speed down to 10 FPS
            // slowing down when getting less FPS than this.
            dt = min(dt, Double(0.1)) // 0.1 -> 10 FPS
            vxapplication_tick(dt)
        } else {
            vxapplication_tick(0.0)
        }
        self.lastTick = now;
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        
        guard let mtkView = self.view as? MTKView else {
            print("🔥 window view should be an MTKView");
            return
        }
        
        mtkView.contentScaleFactor = UIScreen.main.scale
        
        let contentRect = mtkView.frame
        let scale = Float(mtkView.contentScaleFactor)
        let sai = mtkView.safeAreaInsets
        
        var width = UInt32(contentRect.width * CGFloat(scale))
        var height = UInt32(contentRect.height * CGFloat(scale))
        
        // NOTE: Ugly fix for iPhone Pro Max
        // For some reason, render pass height ends up being set to 2435
        // instead of 2436. The app crashes when validating MLL scissor rect.
        // This will certainly be fixed with a bgfx update.
        // In the meantime, this does the job.
        // (fix also used in viewDidLoad)
        if height == 2436 {
            height = 2435
        } else if width == 2436 {
            width = 2435
        }
        
        vxapplication_didResize(width,
                                height,
                                scale,
                                vx_insets(top: Float(sai.top), bottom: Float(sai.bottom), left: Float(sai.left), right: Float(sai.right)))
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(ViewController.showKeyboard(notification:)),
                                               name: UIResponder.keyboardWillShowNotification,
                                               object: nil)
        
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(ViewController.hideKeyboard(notification:)),
                                               name: UIResponder.keyboardWillHideNotification,
                                               object: nil)
    }
    
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge {
        return .all
    }
    
    override var prefersStatusBarHidden: Bool {
        return false
    }
    
    // ------------------------
    // Send touches to C inputs
    // ------------------------
    
    private var touch1: UITouch?
    private var touch1LastPosition = CGPoint(x: 0.0, y: 0.0)
    private var touch1OriginPosition = CGPoint(x: 0.0, y: 0.0)
    
    private var touch2: UITouch?
    private var touch2LastPosition = CGPoint(x: 0.0, y: 0.0)
    private var touch2OriginPosition = CGPoint(x: 0.0, y: 0.0)
    
    private var touch3: UITouch?
    private var touch3LastPosition = CGPoint(x: 0.0, y: 0.0)
    private var touch3OriginPosition = CGPoint(x: 0.0, y: 0.0)
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        
        guard let mtkView = self.view as? MTKView else {
            print("🔥 window view should be an MTKView")
            return
        }        

        var touchID: UInt8
        var pointerID: PointerID
        var touchLocation: CGPoint
        
        for touch in touches {
            
            touchLocation = touch.location(in: mtkView)
            touchLocation.y = mtkView.frame.height - touchLocation.y

            // TODO:
            // - we could disable the touch "per-touch-ID"
            // - we could also support clicks in the safe area (but not drags)
            if touchLocation.y < mtkView.safeAreaInsets.bottom {
                // disableTouchInput = true
                return
            }

            touchLocation.x *= mtkView.contentScaleFactor
            touchLocation.y *= mtkView.contentScaleFactor
            
            if touch1 == nil {
                touch1 = touch
                touch1OriginPosition = touchLocation
                touch1LastPosition = touchLocation
                touchID = 0
                pointerID = PointerIDTouch1
            } else if touch2 == nil {
                touch2 = touch
                touch2OriginPosition = touchLocation
                touch2LastPosition = touchLocation
                touchID = 1
                pointerID = PointerIDTouch2
            } else if touch3 == nil {
                touch3 = touch
                touch3OriginPosition = touchLocation
                touch3LastPosition = touchLocation
                touchID = 2
                pointerID = PointerIDTouch3
            } else {
                // TODO: support 3 touches (allows jump + shoot)
                // more than 2 touches is not supported
                return
            }
            
            postPointerEvent(pointerID, PointerEventTypeDown, Float(touchLocation.x), Float(touchLocation.y), 0.0, 0.0);
            postTouchEvent(touchID, Float(touchLocation.x), Float(touchLocation.y), 0.0, 0.0, TouchStateDown, false)
        }
    }
    
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        
        guard let mtkView = self.view as? MTKView else {
            print("🔥 window view should be an MTKView");
            return
        }
        
        var touchID: UInt8
        var pointerID: PointerID
        var touchLocation: CGPoint
        
        for touch in touches {
            
            touchLocation = touch.location(in: mtkView)
            touchLocation.y = mtkView.frame.height - touchLocation.y
            
            touchLocation.x *= mtkView.contentScaleFactor
            touchLocation.y *= mtkView.contentScaleFactor
            
            if touch == touch1 {
                touch1 = nil
                touchID = 0
                pointerID = PointerIDTouch1
            } else if touch == touch2 {
                touch2 = nil
                touchID = 1
                pointerID = PointerIDTouch2
            } else if touch == touch3 {
                touch3 = nil
                touchID = 2
                pointerID = PointerIDTouch3
            } else {
                // unidentified touch
                continue
            }
            
            postPointerEvent(pointerID, PointerEventTypeUp, Float(touchLocation.x), Float(touchLocation.y), 0.0, 0.0);
            postTouchEvent(touchID, Float(touchLocation.x), Float(touchLocation.y), 0.0, 0.0, TouchStateUp, false)
        }
    }
    
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        
        guard let mtkView = self.view as? MTKView else {
            print("🔥 window view should be an MTKView");
            return
        }
        
        var touchID: UInt8
        var pointerID: PointerID
        var touchLocation: CGPoint
        
        var delta = CGPoint(x: 0, y: 0)
        
        for touch in touches {
            
            touchLocation = touch.location(in: mtkView)
            touchLocation.y = mtkView.frame.height - touchLocation.y
            
            touchLocation.x *= mtkView.contentScaleFactor
            touchLocation.y *= mtkView.contentScaleFactor
            
            if touch == touch1 {
                
                delta.x = touchLocation.x - touch1LastPosition.x
                delta.y = touchLocation.y - touch1LastPosition.y
                touch1LastPosition = touchLocation
                touchID = 0
                pointerID = PointerIDTouch1
                
            } else if touch == touch2 {
                
                delta.x = touchLocation.x - touch2LastPosition.x
                delta.y = touchLocation.y - touch2LastPosition.y
                touch2LastPosition = touchLocation
                touchID = 1
                pointerID = PointerIDTouch2
            
            } else if touch == touch3 {
                
                delta.x = touchLocation.x - touch3LastPosition.x
                delta.y = touchLocation.y - touch3LastPosition.y
                touch3LastPosition = touchLocation
                touchID = 2
                pointerID = PointerIDTouch3
                
            } else {
                // unidentified touch
                continue
            }
            
            postPointerEvent(pointerID, PointerEventTypeMove, Float(touchLocation.x), Float(touchLocation.y), Float(delta.x), Float(delta.y));
            postTouchEvent(touchID, Float(touchLocation.x), Float(touchLocation.y), Float(delta.x), Float(delta.y), TouchStateNone, true)
        }
    }
    
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        
        var touchID: UInt8
        var pointerID: PointerID
        
        for touch in touches {
            if touch == touch1 {
                touch1 = nil
                touchID = 0
                pointerID = PointerIDTouch1
            } else if touch == touch2 {
                touch2 = nil
                touchID = 1
                pointerID = PointerIDTouch2
            } else if touch == touch3 {
                touch3 = nil
                touchID = 2
                pointerID = PointerIDTouch3
            } else {
                // unidentified touch
                continue
            }
            
            postPointerEvent(pointerID, PointerEventTypeCancel, 0.0, 0.0, 0.0, 0.0);
            postTouchEvent(touchID, 0.0, 0.0, 0.0, 0.0, TouchStateCanceled, false)
        }
    }
    
    @objc func showKeyboard(notification: Notification) {
        // taken from https://stackoverflow.com/a/35385550
        // using smaller value
        let THRESHOLD_FOR_HARDWARE_KEYBOARD = 100.0
        
        guard let userInfo = notification.userInfo else {return}
        guard let duration = userInfo[UIResponder.keyboardAnimationDurationUserInfoKey] as? TimeInterval else { return }
        let keyboardEndFrame = (userInfo[UIResponder.keyboardFrameEndUserInfoKey] as! NSValue).cgRectValue

        let hasHardWareKeyboard = keyboardEndFrame.size.height < THRESHOLD_FOR_HARDWARE_KEYBOARD
        
        vxapplication_show_keyboard(vx_size(width: Float(keyboardEndFrame.width), height: hasHardWareKeyboard ? 0.0 : Float(keyboardEndFrame.height)), Float(duration))
    }
    
    @objc func hideKeyboard(notification: Notification) {
        guard let duration = notification.userInfo?[UIResponder.keyboardAnimationDurationUserInfoKey] as? TimeInterval else { return }
        vxapplication_hide_keyboard(Float(duration))
    }
    
    private var _translatedKeys: Any? = nil
    @available(iOS 13.4, *)
    fileprivate var translatedKeys: [UIKeyboardHIDUsage: Input] {
        if _translatedKeys == nil {
            var tKeys = [UIKeyboardHIDUsage: Input]()
            
                // INDEPENDENT OF KEYBOARD LAYOUT
            tKeys[.keyboardReturn] = InputReturn
            tKeys[.keyboardReturnOrEnter] = InputReturn
            tKeys[.keyboardTab] = InputTab
            tKeys[.keyboardSpacebar] = InputSpace
            tKeys[.keyboardDeleteOrBackspace] = InputBackspace
            tKeys[.keyboardDeleteForward] = InputDelete
            tKeys[.keyboardEscape] = InputEsc
            
            // translatedKeys[kVK_Command] // MODIFIER
            tKeys[.keyboardLeftShift] = InputShift // MODIFIER
            tKeys[.keyboardLeftControl] = InputControl // MODIFIER
            tKeys[.keyboardLeftAlt] = InputOption // MODIFIER
            // translatedKeys[kVK_CapsLock] // MODIFIER
            tKeys[.keyboardRightShift] = InputRightShift // MODIFIER
            tKeys[.keyboardRightControl] = InputRightControl // MODIFIER
            tKeys[.keyboardRightAlt] = InputRightOption // MODIFIER
            // translatedKeys[kVK_Function] // MODIFIER
            // translatedKeys[kVK_VolumeUp] // NOT USED
            // translatedKeys[kVK_VolumeDown] // NOT USED
            // translatedKeys[kVK_Mute] // NOT USED
            tKeys[.keyboardF1] = InputF1
            tKeys[.keyboardF2] = InputF2
            tKeys[.keyboardF3] = InputF3
            tKeys[.keyboardF4] = InputF4
            tKeys[.keyboardF5] = InputF5
            tKeys[.keyboardF6] = InputF6
            tKeys[.keyboardF7] = InputF7
            tKeys[.keyboardF8] = InputF8
            tKeys[.keyboardF9] = InputF9
            tKeys[.keyboardF10] = InputF10
            tKeys[.keyboardF11] = InputF11
            tKeys[.keyboardF12] = InputF12
            tKeys[.keyboardF13] = InputF13
            tKeys[.keyboardF14] = InputF14
            tKeys[.keyboardF15] = InputF15
            tKeys[.keyboardF16] = InputF16
            tKeys[.keyboardF17] = InputF17
            tKeys[.keyboardF18] = InputF18
            tKeys[.keyboardF19] = InputF19
            tKeys[.keyboardF20] = InputF20
            // translatedKeys[kVK_Help] // NOT USED
            tKeys[.keyboardHome] = InputHome
            tKeys[.keyboardPageUp] = InputPageUp
            tKeys[.keyboardPageDown] = InputPageDown
            tKeys[.keyboardEnd] = InputEnd
            // translatedKeys[kVK_ForwardDelete] // NOT USED
            tKeys[.keyboardLeftArrow] = InputLeft
            tKeys[.keyboardRightArrow] = InputRight
            tKeys[.keyboardUpArrow] = InputUp
            tKeys[.keyboardDownArrow] = InputDown
            
            // DEPENDENT OF KEYBOARD LAYOUT
            // TODO
            tKeys[.keyboardA] = InputKeyA
            tKeys[.keyboardB] = InputKeyB
            tKeys[.keyboardC] = InputKeyC
            tKeys[.keyboardD] = InputKeyD
            tKeys[.keyboardE] = InputKeyE
            tKeys[.keyboardF] = InputKeyF
            tKeys[.keyboardG] = InputKeyG
            tKeys[.keyboardH] = InputKeyH
            tKeys[.keyboardI] = InputKeyI
            tKeys[.keyboardJ] = InputKeyJ
            tKeys[.keyboardK] = InputKeyK
            tKeys[.keyboardL] = InputKeyL
            tKeys[.keyboardM] = InputKeyM
            tKeys[.keyboardN] = InputKeyN
            tKeys[.keyboardO] = InputKeyO
            tKeys[.keyboardP] = InputKeyP
            tKeys[.keyboardQ] = InputKeyQ
            tKeys[.keyboardR] = InputKeyR
            tKeys[.keyboardS] = InputKeyS
            tKeys[.keyboardT] = InputKeyT
            tKeys[.keyboardU] = InputKeyU
            tKeys[.keyboardV] = InputKeyV
            tKeys[.keyboardW] = InputKeyW
            tKeys[.keyboardX] = InputKeyX
            tKeys[.keyboardY] = InputKeyY
            tKeys[.keyboardZ] = InputKeyZ
            
            // number keys above letters
            tKeys[.keyboard0] = InputKey0
            tKeys[.keyboard1] = InputKey1
            tKeys[.keyboard2] = InputKey2
            tKeys[.keyboard3] = InputKey3
            tKeys[.keyboard4] = InputKey4
            tKeys[.keyboard5] = InputKey5
            tKeys[.keyboard6] = InputKey6
            tKeys[.keyboard7] = InputKey7
            tKeys[.keyboard8] = InputKey8
            tKeys[.keyboard9] = InputKey9
            
            // side keypad
            tKeys[.keypad0] = InputKey0
            tKeys[.keypad1] = InputKey1
            tKeys[.keypad2] = InputKey2
            tKeys[.keypad3] = InputKey3
            tKeys[.keypad4] = InputKey4
            tKeys[.keypad5] = InputKey5
            tKeys[.keypad6] = InputKey6
            tKeys[.keypad7] = InputKey7
            tKeys[.keypad8] = InputKey8
            tKeys[.keypad9] = InputKey9
            
            tKeys[.keypadEqualSign] = InputEqual
            tKeys[.keyboardEqualSign] = InputEqual
            tKeys[.keypadEqualSignAS400] = InputEqual
            tKeys[.keypadHyphen] = InputMinus
            tKeys[.keyboardHyphen] = InputMinus
            tKeys[.keypadPlus] = InputPlus
            tKeys[.keyboardClear] = InputClear
            tKeys[.keyboardClearOrAgain] = InputClear
            tKeys[.keypadPeriod] = InputDecimal
            tKeys[.keyboardPeriod] = InputDecimal
            tKeys[.keypadAsterisk] = InputMultiply
            tKeys[.keypadSlash] = InputDivide
            tKeys[.keyboardSlash] = InputDivide
            tKeys[.keyboardOpenBracket] = InputLeftBracket
            tKeys[.keyboardCloseBracket] = InputRightBracket
            
            tKeys[.keyboardQuote] = InputQuote
            tKeys[.keyboardSemicolon] = InputSemicolon
            tKeys[.keyboardBackslash] = InputBackslash
            tKeys[.keypadComma] = InputComma
            tKeys[.keyboardComma] = InputComma
            tKeys[.keyboardGraveAccentAndTilde] = InputTilde
            
            _translatedKeys = tKeys
        }
        return _translatedKeys as! [UIKeyboardHIDUsage: Input]
    }
    
    @available(iOS 13.4, *)
    fileprivate func handleKeyEvent(key: UIKey) -> (input: Input, characters: String?, modifiers: UInt8) {
        
        var chars = key.characters
        if chars.count > 1 {
            chars = ""
        }
        
        let keyCode: UIKeyboardHIDUsage = key.keyCode
        var input = translatedKeys[keyCode] ?? InputNone
        
        let modifiers: UInt8 = (key.modifierFlags.contains(.shift) ? UInt8(ModifierShift.rawValue) : 0) |
        (key.modifierFlags.contains(.alphaShift) ? UInt8(ModifierShift.rawValue) : 0) |
        (key.modifierFlags.contains(.alternate) ? UInt8(ModifierAlt.rawValue) : 0) |
        (key.modifierFlags.contains(.command) ? UInt8(ModifierSuper.rawValue) : 0) |
        (key.modifierFlags.contains(.control) ? UInt8( ModifierCtrl.rawValue) : 0);
        
        if modifiers & UInt8(ModifierSuper.rawValue) != 0 {
            switch chars {
            case "a":
                input = InputKeyA
            case "v":
                input = InputKeyV
            case "c":
                input = InputKeyC
            case "z": // undo
                input = InputKeyZ
            case "Z": // redo
                input = InputKeyZ
            case "x":
                input = InputKeyX
            default:
                break
            }
            return (input, nil, modifiers)
        }
        
        if modifiers & UInt8(ModifierCtrl.rawValue) != 0 {
            return (input, nil, modifiers)
        }
        
        if modifiers & UInt8(ModifierAlt.rawValue) != 0 {
            if chars == "" {
                switch input {
                case InputKeyN:
                    chars = "~"
                case InputTilde:
                    chars = "`" // same as backtick
                case InputKeyE:
                    chars = "´"
                case InputKeyU:
                    chars = "¨"
                case InputKeyI:
                    chars = "ˆ"
                default:
                    break
                }
            }
        }
        
        return (input, chars, modifiers)
    }
    
    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        var didHandleEvent = false
        for press in presses {
            
            if #available(iOS 13.4, *) {
                
                guard let key = press.key else { continue }
                
                let r = handleKeyEvent(key: key)
                
                var c: UInt32 = 0;
                
                if r.characters != nil {
                    didHandleEvent = true
                    if let str = r.characters {
                        if str.count > 0 {
                            let cStr = str.cString(using: .utf8)
                            c_char32fromUTF8(&c, cStr, nil)
                            postCharEvent(c)
                        }
                    }
                }
                
                if r.input != InputNone {
                    didHandleEvent = true
                    postKeyEvent(r.input, r.modifiers, KeyStateDown)
                    postKeyboardInput(c, r.input, r.modifiers, KeyStateDown)
                }
                
            } else {
                continue
            }
        }
        
        if didHandleEvent == false {
            // Didn't handle this key press, so pass the event to the next responder.
            super.pressesBegan(presses, with: event)
        }
    }
    
    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        var didHandleEvent = false
        for press in presses {
            if #available(iOS 13.4, *) {
                guard let key = press.key else { continue }
                let r = handleKeyEvent(key: key)
                if r.input != InputNone {
                    didHandleEvent = true
                    postKeyEvent(r.input, r.modifiers, KeyStateUp)
                    postKeyboardInput(0, r.input, r.modifiers, KeyStateUp)
                }
            } else {
                continue
            }
        }
        if didHandleEvent == false {
            // Didn't handle this key press, so pass the event to the next responder.
            super.pressesEnded(presses, with: event)
        }
    }
}

extension ViewController: UITextFieldDelegate {

//    // defined here in extension for both UITextFieldDelegate AND UITextViewDelegate
//    func textViewDidChange(_ textView: UITextView) {
//        // TODO
//    }

    func textFieldDidChangeSelection(_ textField: UITextField) {
        guard let textField = textField as? CustomTextField else {
            return
        }
        if textField.isDidChangeTurnedOff { return }

        guard let gcaRange = getTextFieldGraphemeClusterAwareRange(textField: textField) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: textField.text!, start: gcaRange.start, end: gcaRange.end) else { return }

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
        }
    }

    func textFieldShouldReturn(_ textField: UITextField) -> Bool {
        switch textField.returnKeyType {
        case .default:
            return false;
        case .done:
            hostPlatformTextInputDone();
            return false;
        case .send:
            hostPlatformTextInputDone();
            return false;
        case .next:
            hostPlatformTextInputNext();
            return false;
        default:
            textField.resignFirstResponder()
            return true
        }
    }

    func textFieldDidEndEditing(_ textField: UITextField) {
        hostPlatformTextInputClose();
    }

    func textFieldDidEndEditing(_ textField: UITextField, reason: UITextField.DidEndEditingReason) {
        hostPlatformTextInputClose();
    }
}

extension ViewController: UITextViewDelegate {

    func textViewDidChangeSelection(_ textView: UITextView) {
        guard let textView = textView as? CustomTextView else {
            return
        }
        if textView.isDidChangeTurnedOff { return }

        guard let gcaRange = getGraphemeClusterAwareRange(textView: textView) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: textView.text, start: gcaRange.start, end: gcaRange.end) else { return }

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
        }
    }

    func textViewDidBeginEditing(_ textView: UITextView) {
        // nothing to do
    }

    func textViewDidEndEditing(_ textView: UITextView) {
        hostPlatformTextInputClose();
    }
}

class CustomTextView: UITextView {
    var isDidChangeTurnedOff: Bool = false
}

class CustomTextField: UITextField {
    var isDidChangeTurnedOff: Bool = false
}

func getGraphemeClusterAwareRange(textView: UITextView) -> (start: Int, end: Int)? {
    // Get the NSRange from the NSTextView
    let nsRange = textView.selectedRange

    // Convert NSRange to Swift String range
    guard let range = Range(nsRange, in: textView.text) else {
        return nil
    }

    // Convert String.Index to Int
    let start = textView.text.distance(from: textView.text.startIndex, to: range.lowerBound)
    let end = textView.text.distance(from: textView.text.startIndex, to: range.upperBound)

    return (start, end)
}

func setGraphemeClusterAwareRange(textView: UITextView, string: String, start: Int, end: Int) -> Bool {
    // Convert Int offsets to String.Index
    guard let startIndex = string.index(string.startIndex, offsetBy: start, limitedBy: string.endIndex),
          let endIndex = string.index(string.startIndex, offsetBy: end, limitedBy: string.endIndex) else {
        return false
    }

    // Ensure the range is valid
    guard startIndex <= endIndex else {
        return false
    }

    // Convert String.Index range to NSRange
    let range = NSRange(startIndex..<endIndex, in: string)

    // Set the NSRange in the textView
    textView.selectedRange = range

    return true
}

func getTextFieldGraphemeClusterAwareRange(textField: UITextField) -> (start: Int, end: Int)? {
    guard let textRange = textField.selectedTextRange,
          let text = textField.text else {
        return nil
    }

    // Convert UITextRange to NSRange
    let startPosition = textField.offset(from: textField.beginningOfDocument, to: textRange.start)
    let endPosition = textField.offset(from: textField.beginningOfDocument, to: textRange.end)

    // Convert NSRange to Swift String range
    guard let range = Range(NSRange(location: startPosition, length: endPosition - startPosition), in: text) else {
        return nil
    }

    // Convert String.Index to Int
    let start = text.distance(from: text.startIndex, to: range.lowerBound)
    let end = text.distance(from: text.startIndex, to: range.upperBound)

    return (start, end)
}


func setTextFieldGraphemeClusterAwareRange(textField: UITextField, string: String, start: Int, end: Int) -> Bool {
    // Convert Int offsets to String.Index
    guard let startIndex = string.index(string.startIndex, offsetBy: start, limitedBy: string.endIndex),
          let endIndex = string.index(string.startIndex, offsetBy: end, limitedBy: string.endIndex) else {
        return false
    }

    // Ensure the range is valid
    guard startIndex <= endIndex else {
        return false
    }

    // Convert String.Index range to UITextRange
    let beginning = textField.beginningOfDocument
    guard let startTextPosition = textField.position(from: beginning, offset: string.distance(from: string.startIndex, to: startIndex)),
          let endTextPosition = textField.position(from: beginning, offset: string.distance(from: string.startIndex, to: endIndex)) else {
        return false
    }

    // Set the UITextRange in the UITextField
    let textRange = textField.textRange(from: startTextPosition, to: endTextPosition)
    textField.selectedTextRange = textRange

    return true
}
