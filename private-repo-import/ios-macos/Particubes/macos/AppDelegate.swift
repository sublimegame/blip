//
//  AppDelegate.swift
//  macos
//
//  Created by Adrien Duermael on 2/12/20.
//

import Cocoa

class DisplayContext {
    static let shared = DisplayContext()

    var lastTick: DispatchTime?

    var appIsActive: Bool = false
    var isMouseHidden: Bool = false
    var skipMovements: Int = 0
    var windowResizing: Bool = false
    var windowMoving: Bool = false
    var doneResizingTimer: Double = 0.0

    private let lock = NSLock()
    private var tickProcessing: Bool = false

    func startTick() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        if tickProcessing {
            return false
        }
        tickProcessing = true
        return true
    }

    func endTick() {
        lock.lock()
        defer { lock.unlock() }
        tickProcessing = false
    }

    var window: NSWindow! = nil

    func doneMoving() {
        windowMoving = false
    }

    func doneResizing() {
        let displayContext = DisplayContext.shared
        if displayContext.windowResizing == false { return }
        displayContext.windowResizing = false

        if window == nil {
            print("🔥 can't get window's content view frame")
            return
        }

        guard let contentRect = window.contentView?.frame.standardized else {
            print("🔥 can't get window's content view frame")
            return
        }

        let scale: CGFloat = window.backingScaleFactor

        vxapplication_didResize(UInt32(contentRect.width * scale),
                                UInt32(contentRect.height * scale),
                                Float(scale),
                                vx_insets(top: 0.0, bottom: 0.0, left: 0.0, right: 0.0))
    }

    private init() {}
}

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {
    static var currentInstance: AppDelegate?

    private let textField = CustomTextField(frame: NSRect(x: 0, y: 10000, width: 1000, height: 50))
    private let textView = CustomTextView(frame: NSRect(x: 0, y: 10000, width: 1000, height: 100)) // multiline

    private var mainExecutionQueue: DispatchQueue! = nil

    // App window minimum size
    private static let WINDOWS_MINIMUM_WIDTH: CGFloat = 320
    private static let WINDOWS_MINIMUM_HEIGHT: CGFloat = 320
    
    // time to trigger doneResizing / doneMoving
    private static let DONE_RESIZING_OR_MOVING_TIME: Double = 0.1
    
    @IBOutlet weak var window: NSWindow!

    // VSYNC WIP
    private var displayLink: CVDisplayLink?

    private var localMonitor: Any?
    private var globalMonitor: Any?
    
    private var m_device: MTLDevice?

    /// name of the Voxowl particubes worlds app group
    private let voxowlAppGroupName = "com.voxowl.particubes"

    /// push token type for Apple platformes
    private let pushTokenTypeApple = "apple"

    // TEXT INPUTS

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

        guard let self = AppDelegate.currentInstance else { return }

        if multiline {
            DispatchQueue.main.async {
                guard let window = self.textView.window else { return }

                self.textView.isAutomaticTextReplacementEnabled = false
                self.textView.isAutomaticSpellingCorrectionEnabled = false
                self.textView.isAutomaticDataDetectionEnabled = false
                self.textView.isAutomaticLinkDetectionEnabled = false
                self.textView.isAutomaticTextCompletionEnabled = false
                self.textView.isAutomaticDashSubstitutionEnabled = false
                self.textView.isAutomaticQuoteSubstitutionEnabled = false
                self.textView.isContinuousSpellCheckingEnabled = false

                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                if strDidChange {
                    self.textView.isDidChangeTurnedOff = true
                    self.textView.string = s.string
                    self.textView.isDidChangeTurnedOff = false
                }

                window.makeFirstResponder(self.textView)

                self.textView.isDidChangeTurnedOff = true
                _ = setGraphemeClusterAwareRange(in: self.textView, string: s.string, start: s.start, end: s.end)
                self.textView.isDidChangeTurnedOff = false
            }
        } else {
            DispatchQueue.main.async {
                guard let window = self.textField.window else { return }

                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

//                self.textField.contentType = .telephoneNumber
//                self.textField.contentType = .oneTimeCode

                if strDidChange {
                    self.textField.isDidChangeTurnedOff = true
                    self.textField.stringValue = s.string
                    self.textField.isDidChangeTurnedOff = false
                }

                window.makeFirstResponder(self.textField)

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

        guard let self = AppDelegate.currentInstance else { return }


        DispatchQueue.main.async {
            guard let window = self.textView.window else { return }

            if window.firstResponder == self.textView {
                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }
                // print("UPDATE \(cursorStart)-\(cursorEnd) -> \(s.start)-\(s.end)")

                self.textView.isDidChangeTurnedOff = true
                if strDidChange {
                    self.textView.string = s.string
                }
                _ = setGraphemeClusterAwareRange(in: self.textView, string: s.string, start: s.start, end: s.end)
                self.textView.isDidChangeTurnedOff = false

            } else if window.firstResponder == self.textField.currentEditor() {
                guard let s = utf8PointerAndByteRangeToStringAndRange(utf8Data: data, start: cursorStart, end: cursorEnd) else { return }

                self.textField.isDidChangeTurnedOff = true
                if strDidChange {
                    self.textField.stringValue = s.string
                }
                _ = setTextFieldGraphemeClusterAwareRange(textField: self.textField, string: s.string, start: s.start, end: s.end)
                self.textField.isDidChangeTurnedOff = false
            }
        }
    }

    static func textInputActionCallback(action: TextInputAction) {
        guard let self = AppDelegate.currentInstance else { return }
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
                guard let window = self.textView.window else { return }
                if window.firstResponder == self.textView {
                    window.makeFirstResponder(nil)
                    return
                }
                if let editor = self.textField.currentEditor(), window.firstResponder == editor {
                    window.makeFirstResponder(nil)
                    return
                }
            default:
                return
            }
        }
    }

    /// example: "/Users/gaetan/Library/Group Containers/com.voxowl.particubes"
    private func getAppGroupStoragePath() -> String? {
        let url = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: voxowlAppGroupName)
        let result: String? = url?.path
        if let path = result {
            if FileManager.default.fileExists(atPath: path) == false {
                do {
                    let attr: [FileAttributeKey: Any] = [FileAttributeKey.posixPermissions: 0o700]
                    try FileManager.default.createDirectory(atPath: path, withIntermediateDirectories: true, attributes: attr)
                } catch {
                    print("failed to create app group container directory")
                    return nil
                }
            }
            return url?.path
        } else {
            return nil
        }
    }
    
    // resets mouse to center, always, to avoid leaving the game
    static func centerMouse(window: NSWindow?) {
        
        guard let window = window else {
            return
        }
        
        guard let view = window.contentView else {
            return
        }
        
        let point = NSPoint(x: view.frame.width * 0.5, y: view.frame.height * 0.5)
        let pointInWindow = view.convert(point, to: nil)
        let pointOnScreen = window.convertPoint(toScreen: pointInWindow)
        if let screenHeight = window.screen?.frame.height {
            let cgPoint = CGPoint(x: pointOnScreen.x, y: screenHeight - pointOnScreen.y )
            CGWarpMouseCursorPosition(cgPoint)
        }
    }

    static func mouseInWindow(_ window: NSWindow) -> Bool {
        
        let mouseWindowLocation = window.mouseLocationOutsideOfEventStream
        
        let mouseScreenLocation = window.convertPoint(toScreen: mouseWindowLocation)
        
        let eventWindowNumber = NSWindow.windowNumber(at: mouseScreenLocation, belowWindowWithWindowNumber: 0)
        
        if eventWindowNumber != window.windowNumber {
            return false
        }
        
        return true
    }

    static func mouseInContentView(_ window: NSWindow) -> Bool {

        if AppDelegate.mouseInWindow(window) == false { return false }

        guard let contentView = window.contentView else { return false }

        let globalMouseLocation = NSEvent.mouseLocation
        let windowMouseLocation = window.convertPoint(fromScreen: globalMouseLocation)
        let viewMouseLocation = contentView.convert(windowMouseLocation, from: nil)

        if (NSPointInRect(viewMouseLocation, contentView.bounds) == false) {
            // mouse not in content view
            return false // do not consume event
        }

        return true
    }

    var absPreviousDeltaY: Float = 0.0

    let displayLinkCallbackRef: CVDisplayLinkOutputCallback = { (displayLink: CVDisplayLink,
                                                              inNow: UnsafePointer<CVTimeStamp>,
                                                              inOutputTime: UnsafePointer<CVTimeStamp>,
                                                              flagsIn: CVOptionFlags,
                                                              flagsOut: UnsafeMutablePointer<CVOptionFlags>,
                                                              context: UnsafeMutableRawPointer?) -> CVReturn in

        guard DisplayContext.shared.startTick() else {
            // Previous processing not done, skip this tick
            return kCVReturnSuccess
        }

        DispatchQueue.main.async {

            let now = DispatchTime.now()

            let displayContext = DisplayContext.shared

            if let lastTick = displayContext.lastTick {
                let nanoTime = now.uptimeNanoseconds - lastTick.uptimeNanoseconds
                var dt : Double = Double(nanoTime) / Double(1_000_000_000) // in seconds
                dt = min(dt, Double(0.03333))
                vxapplication_tick(dt)

                if displayContext.doneResizingTimer > 0.0 {
                    displayContext.doneResizingTimer -= dt
                    if displayContext.doneResizingTimer <= 0.0 {
                        displayContext.doneResizingTimer = 0.0
                        displayContext.doneResizing()
                    }
                }
            }

            if displayContext.appIsActive &&
                displayContext.windowMoving == false &&
                displayContext.windowResizing == false &&
                displayContext.window != nil {

                let mouseHidden = vx_is_mouse_hidden()
                if displayContext.isMouseHidden != mouseHidden {
                    if AppDelegate.mouseInContentView(displayContext.window) {
                        displayContext.isMouseHidden = mouseHidden
                        if (mouseHidden) {
                            NSCursor.hide()
                            displayContext.skipMovements = 2
                            AppDelegate.centerMouse(window: displayContext.window)
                        } else {
                            NSCursor.unhide()
                        }
                    }
                }
            }

            displayContext.lastTick = now
            displayContext.endTick()
        }
        return kCVReturnSuccess
    }

    func setupDisplayLink() {
        // Create the display link
        var result = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink)
        if result != kCVReturnSuccess {
            print("Error creating display link")
            return
        }

        // Set the output callback
        result = CVDisplayLinkSetOutputCallback(displayLink!, displayLinkCallbackRef, nil)
        if result != kCVReturnSuccess {
            print("Error setting display link callback")
            return
        }

        // Start the display link
        CVDisplayLinkStart(displayLink!)
    }

    func stopDisplayLink() {
        if let dl = displayLink {
            CVDisplayLinkStop(dl)
        }
    }


    func application(_ application: NSApplication, open urls: [URL]) {
        if urls.count == 0 { return }
        let url = urls[0]
        _ = vxapplication_openURL(url.absoluteString)
    }

    func applicationDidFinishLaunching(_ aNotification: Notification) {

        vxapplication_init()

        mainExecutionQueue = DispatchQueue(label: "cubzh-queue")

        // from : https://stackoverflow.com/questions/44684031/swift-3-how-to-supply-a-c-style-void-object-to-ios-frameworks-for-use-with
        // let windowUnmanaged: Unmanaged = Unmanaged.passRetained(self.window)
        // let windowPtr: UnsafeMutableRawPointer = windowUnmanaged.toOpaque()

        // using custom CAMetalLayer to avoid deadlock with multithreaded BGFX:
        // https://github.com/bkaradzic/bgfx/issues/1773#issuecomment-663914069
        // https://github.com/RavEngine/RavEngine/commit/a5b638f3260c1c758ee30362fe34fbc46105bab4#diff-dc917ce0984c8cd01d4c9323bed3977cb01d4b36db96f0beec1acb29d302e7afR10

        guard let contentView = self.window.contentView else {
            return
        }
        contentView.wantsLayer = true

        self.window.initialFirstResponder = contentView

        let gameView = NSView()
        gameView.translatesAutoresizingMaskIntoConstraints = false
        let layer = CAMetalLayer()
        gameView.layer = layer

        contentView.addSubview(gameView)
        NSLayoutConstraint.activate([
            gameView.topAnchor.constraint(equalTo: contentView.topAnchor),
            gameView.bottomAnchor.constraint(equalTo: contentView.bottomAnchor),
            gameView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            gameView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor)
        ])

        let layerUnmanaged: Unmanaged = Unmanaged.passRetained(layer)
        let layerPtr: UnsafeMutableRawPointer = layerUnmanaged.toOpaque()

        DisplayContext.shared.window = self.window;

        AppDelegate.currentInstance = self

        contentView.addSubview(textField)
        // resigns automatically when hidden,
        // placing text view far from visible area instead
        // textView.isHidden = true
        textField.isEditable = true
        textField.isEnabled = true
        textField.isSelectable = true

        textView.allowsUndo = true
        textView.delegate = self
        contentView.addSubview(textView)
        // resigns automatically when hidden,
        // placing text view far from visible area instead
        // textView.isHidden = true
        textView.isEditable = true
        textView.isSelectable = true

        NotificationCenter.default.addObserver(forName: NSWindow.didBecomeKeyNotification, object: self.window, queue: .main) { notification in
            print("🤔 window did become key - current first responder: \(String(describing: self.window.firstResponder))")
        }

        NotificationCenter.default.addObserver(forName: NSWindow.didResignKeyNotification, object: self.window, queue: .main) { notification in
            print("🤔 window did resign key - current first responder: \(String(describing: self.window.firstResponder))")
        }

        guard let contentRect = self.window.contentView?.frame.standardized else {
            print("🔥 can't get window's content view frame")
            return
        }
        
        m_device = MTLCreateSystemDefaultDevice()
        
        let deviceUnmanaged: Unmanaged = Unmanaged.passRetained(m_device!)
        let devicePtr: UnsafeMutableRawPointer = deviceUnmanaged.toOpaque()
        
        let scale: CGFloat = self.getWindowBackingScaleFactor()
        

        vxapplication_didFinishLaunching(layerPtr,
                                         devicePtr,
                                         UInt32(contentRect.width * scale),
                                         UInt32(contentRect.height * scale),
                                         Float(scale),
                                         vx_insets(top: 0.0, bottom: 0.0, left: 0.0, right: 0.0),
                                         ""); // world ID | ex : {"worldID":"00b160e8-2aa2-43b0-821b-03daa8f50c41"}

        textinputRequestCallbackPtr = {(str: UnsafePointer<CChar>?, strLen: Int, strDidChange: Bool, cursorStart: Int, cursorEnd: Int, multiline: Bool, keyboardType: TextInputKeyboardType, returnKeyType: TextInputReturnKeyType, suggestions: Bool) -> Void in

            guard let safeStr = str else {
                return
            }
            let data = Data(bytes: safeStr, count: strLen)

            AppDelegate.textInputRequestCallback(data: data, strDidChange: strDidChange, cursorStart: cursorStart, cursorEnd: cursorEnd, multiline: multiline, keyboardType: keyboardType, returnKeyType: returnKeyType, suggestions: suggestions)
        }

        textinputUpdateCallbackPtr = {(str: UnsafePointer<CChar>?, strLen: Int, strDidChange: Bool, cursorStart: Int, cursorEnd: Int) -> Void in

            guard let safeStr = str else {
                return
            }
            let data = Data(bytes: safeStr, count: strLen)

            AppDelegate.textInputUpdateCallback(data: data, strDidChange: strDidChange, cursorStart: cursorStart, cursorEnd: cursorEnd)
        }

        textinputActionCallbackPtr = { (action: TextInputAction) in
            AppDelegate.textInputActionCallback(action: action)
        }

        hostPlatformTextInputRegisterDelegate(textinputRequestCallbackPtr,
                                              textinputUpdateCallbackPtr,
                                              textinputActionCallbackPtr);

        setupDisplayLink()
        
        self.globalMonitor = NSEvent.addGlobalMonitorForEvents(matching: [.mouseMoved, .scrollWheel], handler: { [weak self] (event) -> Void in
            
            guard let strongSelf = self else { return }

            let displayContext = DisplayContext.shared

            // only consider global events when the app is not active
            if displayContext.appIsActive { return }
            if displayContext.windowMoving || displayContext.windowResizing { return }

            let eventType = event.type
           
            if AppDelegate.mouseInContentView(strongSelf.window) == false { return }

            switch eventType {
            case .mouseMoved:
                // do not consider mouse movements when the app is not focused
                // IF the mouse is supposed to be hidden in the game.
                // We still want to consider the mouse for UI interaction even if
                // the app is not focused (buttons hovering, scrolls, etc.)
                // Using vx_is_mouse_hidden to define what the game wants
                // since isMouseHidden is always false when the app is not active.
                if vx_is_mouse_hidden() { return }
                
                if displayContext.skipMovements > 0 {
                    displayContext.skipMovements -= 1
                    return
                }

                postPointerEvent(PointerIDMouse, // not specifying if a button is down
                                 PointerEventTypeMove,
                                 Float(strongSelf.window.mouseLocationOutsideOfEventStream.x * strongSelf.window.backingScaleFactor),
                                 Float(strongSelf.window.mouseLocationOutsideOfEventStream.y * strongSelf.window.backingScaleFactor),
                                 Float(event.deltaX * strongSelf.window.backingScaleFactor),
                                 -Float(event.deltaY * strongSelf.window.backingScaleFactor))
                
                postMouseEvent(
                    Float(strongSelf.window.mouseLocationOutsideOfEventStream.x * strongSelf.window.backingScaleFactor),
                    Float(strongSelf.window.mouseLocationOutsideOfEventStream.y * strongSelf.window.backingScaleFactor),
                    Float(event.deltaX),
                    -Float(event.deltaY),
                    MouseButtonNone,
                    false /* down (doesn't matter) */,
                    true /* move */
                )
                break // mouseMoved
            case .scrollWheel:
                
                if AppDelegate.mouseInWindow(strongSelf.window) == false { return }
                
                switch event.phase {
                case .began:
                    // nothing to do
                    break
                case .cancelled:
                    // nothing to do
                    break
                case .changed:
                    postMouseEvent(
                        0.0,
                        0.0,
                        Float(event.scrollingDeltaX),
                        Float(event.scrollingDeltaY),
                        MouseButtonScroll,
                        false /* down */,
                        false); /* move */
                    break
                case .ended:
                    // nothing to do
                    break
                case .mayBegin:
                    // nothing to do
                    break
                case .stationary:
                    // nothing to do
                    break
                default:
                    // force stop if delta is set to 0 manually (touch down)
                    if (Float(strongSelf.absPreviousDeltaY) > EPSILON_ZERO && event.scrollingDeltaY == 0.0) {

                        postMouseEvent(
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            MouseButtonScroll,
                            false /* down */,
                            false); /* move */
                        
                        strongSelf.absPreviousDeltaY = 0.0
                    } else {
                        strongSelf.absPreviousDeltaY = fabsf(Float(event.scrollingDeltaY))
                    }
                }
                
                break // scrollWheel
            default:
                break
            }
        })

        self.localMonitor = NSEvent.addLocalMonitorForEvents(matching: .any) { [weak self] (event) -> NSEvent? in

            guard let strongSelf = self else {
                return event // do not consume event
            }

            let displayContext = DisplayContext.shared

            let eventType = event.type
            
            if displayContext.windowMoving {
                if eventType == .leftMouseUp {
                    displayContext.doneMoving()
                }
                return event // do not consume event
            }
            
            if displayContext.windowResizing {
                return event // do not consume event
            }
            
            // commented this so we DO NOT ignore mouse events happening outside of the app windows
            // switch eventType {
            // case .mouseMoved, .leftMouseDragged,
            //         .rightMouseDragged, .otherMouseDragged,
            //         .leftMouseDown, .rightMouseDown, .otherMouseDown:
            //     if strongSelf.mouseInContentView() == false {
            //         // return event // do not consume event
            //     }
            //     break
            // default:
            //     break
            // }

            // handle key events
            switch eventType {
            case .flagsChanged:
                let flags = event.modifierFlags
                let modifiers: UInt8 = (flags.contains(.shift) ? UInt8(ModifierShift.rawValue) : 0) |
                    (flags.contains(.option) ? UInt8(ModifierAlt.rawValue) : 0) |
                    (flags.contains(.command) ? UInt8(ModifierSuper.rawValue) : 0) |
                    (flags.contains(.control) ? UInt8( ModifierCtrl.rawValue) : 0)

                let r = handleKeyEvent(e: event)

                switch r.input {
                case InputShift:
                    if flags.contains(.shift) {
                        postKeyEvent(InputShift, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputShift, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputShift, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputShift, modifiers, KeyStateUp)
                    }
                    break
                case InputRightShift:
                    if flags.contains(.shift) {
                        postKeyEvent(InputRightShift, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputRightShift, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputRightShift, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputRightShift, modifiers, KeyStateUp)
                    }
                    break
                case InputControl:
                    if flags.contains(.control) {
                        postKeyEvent(InputControl, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputControl, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputControl, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputControl, modifiers, KeyStateUp)
                    }
                    break
                case InputRightControl:
                    if flags.contains(.control) {
                        postKeyEvent(InputRightControl, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputRightControl, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputRightControl, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputRightControl, modifiers, KeyStateUp)
                    }
                    break
                case InputOption:
                    if flags.contains(.option) {
                        postKeyEvent(InputOption, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputOption, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputOption, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputOption, modifiers, KeyStateUp)
                    }
                    break
                case InputRightOption:
                    if flags.contains(.option) {
                        postKeyEvent(InputRightOption, modifiers, KeyStateDown)
                        postKeyboardInput(0, InputRightOption, modifiers, KeyStateDown)
                    } else {
                        postKeyEvent(InputRightOption, modifiers, KeyStateUp)
                        postKeyboardInput(0, InputRightOption, modifiers, KeyStateUp)
                    }
                    break
                default:
                    break
                }

                break
            case .keyDown:
                guard let self = AppDelegate.currentInstance else { return event }
                guard let window = self.textView.window else { return event }

                let r = handleKeyEvent(e: event)

                if (r.input != InputEsc) {
                    if window.firstResponder == self.textView { return event }
                    if window.firstResponder == self.textField.currentEditor() {
                        if r.input == InputReturn || r.input == InputReturnKP {
                            hostPlatformTextInputDone()
                            return nil // capture
                        }
                        if (r.input != InputDown && r.input != InputUp) {
                            return event
                        }
                    }
                }

                var c: UInt32 = 0;
                
                if let str = r.characters {
                    if str.count > 0 {
                        let cStr = str.cString(using: .utf8)
                        c_char32fromUTF8(&c, cStr, nil)
                        postCharEvent(c)
                    }
                }
                
                if r.input != InputNone {
                    postKeyEvent(r.input, r.modifiers, KeyStateDown)
                    postKeyboardInput(c, r.input, r.modifiers, KeyStateDown)
                    
                    // do not consume event when down key is a modifier
                    if (r.modifiers & UInt8(ModifierSuper.rawValue) > 0) {
                        return event // do not consume event
                    } else {
                        return nil // consume event
                    }
                }
                break
            case .keyUp:
                guard let self = AppDelegate.currentInstance else { return event }
                guard let window = self.textView.window else { return event }
                if window.firstResponder == self.textView { return event }
                if window.firstResponder == self.textField.currentEditor() { return event }

                let r = handleKeyEvent(e: event)
                if r.input != InputNone {
                    postKeyEvent(r.input, r.modifiers, KeyStateUp)
                    postKeyboardInput(0, r.input, r.modifiers, KeyStateUp)
                    return nil // consume event
                }
                break
            case .mouseMoved, .leftMouseDragged, .rightMouseDragged, .otherMouseDragged:

                if displayContext.skipMovements > 0 {
                    displayContext.skipMovements -= 1
                    return event // do not consume event
                }
                
                // NOTE: event.window might become nil when receiving notifications
                // using self.window instead, we did not notice any differences.
                // It doesn't solve the issue when moving the app to a different screen though.
                guard let window = strongSelf.window else {
                    return event
                }
                
                let deltaX: CGFloat = window.currentEvent?.deltaX ?? event.deltaX
                let deltaY: CGFloat = window.currentEvent?.deltaY ?? event.deltaY
                   
                // PointerIDMouse mouse by default (move event, not drag)
                var pointerID = PointerIDMouse
                if (eventType == .leftMouseDragged) {
                    pointerID = PointerIDMouseButtonLeft
                } else if (eventType == .rightMouseDragged) {
                    pointerID = PointerIDMouseButtonRight
                }
                
                postPointerEvent(pointerID,
                                 PointerEventTypeMove,
                                 Float(window.mouseLocationOutsideOfEventStream.x * strongSelf.window.backingScaleFactor),
                                 Float(window.mouseLocationOutsideOfEventStream.y * strongSelf.window.backingScaleFactor),
                                 Float(event.deltaX * window.backingScaleFactor),
                                 -Float(event.deltaY * window.backingScaleFactor))

                
                postMouseEvent(
                    Float(window.mouseLocationOutsideOfEventStream.x * window.backingScaleFactor),
                    Float(window.mouseLocationOutsideOfEventStream.y * window.backingScaleFactor),
                    Float(deltaX * window.backingScaleFactor),
                    -Float(deltaY * window.backingScaleFactor),
                    MouseButtonNone,
                    false, // down (doesn't matter)
                    true /* move */
                )
                
                if displayContext.isMouseHidden {
                    AppDelegate.centerMouse(window: window)
                }
                
                return event // do not consume event
                
            case .leftMouseDown:
                guard let window = event.window else {
                    return event
                }
                
                postPointerEvent(PointerIDMouseButtonLeft,
                                 PointerEventTypeDown,
                                 Float(event.locationInWindow.x * window.backingScaleFactor),
                                 Float(event.locationInWindow.y * window.backingScaleFactor),
                                 0.0,
                                 0.0)
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonLeft,
                    true /* down */,
                    false /* move */
                )
                return event

            case .leftMouseUp:
                guard let window = event.window else {
                    return event
                }
                
                postPointerEvent(PointerIDMouseButtonLeft,
                                 PointerEventTypeUp,
                                 Float(event.locationInWindow.x * window.backingScaleFactor),
                                 Float(event.locationInWindow.y * window.backingScaleFactor),
                                 0.0,
                                 0.0)
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonLeft,
                    false /* down */,
                    false /* move */
                )
                
                return event

            case .rightMouseDown:
                
                guard let window = event.window else {
                    return event
                }
                
                postPointerEvent(PointerIDMouseButtonRight,
                                 PointerEventTypeDown,
                                 Float(event.locationInWindow.x * window.backingScaleFactor),
                                 Float(event.locationInWindow.y * window.backingScaleFactor),
                                 0.0,
                                 0.0)
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonRight,
                    true /* down */,
                    false /* move */
                )
                return event

            case .rightMouseUp:
                
                guard let window = event.window else {
                    return event
                }
                
                postPointerEvent(PointerIDMouseButtonRight,
                                 PointerEventTypeUp,
                                 Float(event.locationInWindow.x * window.backingScaleFactor),
                                 Float(event.locationInWindow.y * window.backingScaleFactor),
                                 0.0,
                                 0.0)
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonRight,
                    false /* down */,
                    false /* move */
                )
                return event

            case .otherMouseDown:
                
                guard let window = event.window else {
                    return event
                }
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonMiddle,
                    true /* down */,
                    false /* move */
                )
                return event

            case .otherMouseUp:
                
                guard let window = event.window else {
                    return event
                }
                
                postMouseEvent(
                    Float(event.locationInWindow.x * window.backingScaleFactor),
                    Float(event.locationInWindow.y * window.backingScaleFactor),
                    0.0,
                    0.0,
                    MouseButtonMiddle,
                    false /* down */,
                    false /* move */
                )
                return event
                
            case .scrollWheel:
                switch event.phase {
                case .began:
                    // nothing to do
                    break
                case .cancelled:
                    // nothing to do
                    break
                case .changed:
                    
                    // NOTE: this is not always called when using a mouse
                    // other than the magic mouse or magic trackpad.
                    // When using those devices, "changed" phase happens
                    // while moving the finger on the surface.
                    // But then, it continues with "default" case, for inertia.
                    // Other mouses only enter in the "default" case when
                    // activating mouse's wheel.
                    
                    guard let window = event.window else {
                        return event
                    }
                    
                    postPointerEvent(PointerIDMouse, PointerEventTypeWheel,
                                     Float(event.locationInWindow.x * window.backingScaleFactor),
                                     Float(event.locationInWindow.y * window.backingScaleFactor),
                                     Float(event.scrollingDeltaX),
                                     Float(event.scrollingDeltaY));
                    
                    postMouseEvent(
                        Float(event.locationInWindow.x * window.backingScaleFactor),
                        Float(event.locationInWindow.y * window.backingScaleFactor),
                        Float(event.scrollingDeltaX),
                        Float(event.scrollingDeltaY),
                        MouseButtonScroll,
                        false /* down */,
                        false); /* move */
                    
                    // SHOULD CONSUME EVENT?
                    
                    break
                case .ended:
                    // nothing to do
                    break
                case .mayBegin:
                    // nothing to do
                    break
                case .stationary:
                    // nothing to do
                    break
                default:
                    // force stop if delta is set to 0 manually (touch down)
                    if (Float(strongSelf.absPreviousDeltaY) > EPSILON_ZERO && event.scrollingDeltaY == 0.0) {
                        
                        postPointerEvent(PointerIDMouse, PointerEventTypeWheel,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0);
                        
                        postMouseEvent(
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            MouseButtonScroll,
                            false /* down */,
                            false); /* move */
                        
                        strongSelf.absPreviousDeltaY = 0.0
                    } else {
                        strongSelf.absPreviousDeltaY = fabsf(Float(event.scrollingDeltaY))
                        
                        guard let window = event.window else {
                            return event
                        }
                        
                        postPointerEvent(PointerIDMouse, PointerEventTypeWheel,
                                         Float(event.locationInWindow.x * window.backingScaleFactor),
                                         Float(event.locationInWindow.y * window.backingScaleFactor),
                                         Float(event.scrollingDeltaX),
                                         Float(event.scrollingDeltaY));
                        
                        postMouseEvent(
                            Float(event.locationInWindow.x * window.backingScaleFactor),
                            Float(event.locationInWindow.y * window.backingScaleFactor),
                            Float(event.scrollingDeltaX),
                            Float(event.scrollingDeltaY),
                            MouseButtonScroll,
                            false /* down */,
                            false); /* move */
                    }
                }
                
            default:
                return event
            }
            
            // do not consume event when reaching that point
            return event
        }
    }

    private var nextNotification = 0
    func applicationDidBecomeActive(_ notification: Notification) {
        vxapplication_didBecomeActive()

        // DEBUG:
        //        let configs = [
        //            ("Cha-ching!", "Here's 100 coins!", "money", 42),
        //            ("Yay!", "newbie liked one of your items", "like", 42),
        //            ("Hurry up!", "2 hours left to collect your daily rewards!", "generic", 42),
        //            ("Friend request!", "Notilos wants to be your friend.", "social", 42)
        //        ]
        //
        //        let e = configs[nextNotification]
        //        vxapplication_didReceiveNotification(e.0, e.1, e.2, UInt32(e.3))
        //        nextNotification += 1
        //        if nextNotification >= configs.count {
        //            nextNotification = 0;
        //        }

        let displayContext = DisplayContext.shared
        displayContext.appIsActive = true
        displayContext.isMouseHidden = false

        NSCursor.unhide() // safety measure, but should be ok
        
        //window.setContentSize(NSSize(width: 700, height: 500)) // laptop
        //window.setContentSize(NSSize(width: 450, height: 750)) // smartphone
        //window.setContentSize(NSSize(width: 600, height: 400)) // tablet
        //window.setContentSize(NSSize(width: 1440, height: 810)) // smaller (for thumbnails)
        //window.setContentSize(NSSize(width: 1600, height: 900))
        //window.styleMask = .borderless
        //window.hasShadow = false
        //window.center()
    }
    
    func applicationWillResignActive(_ notification: Notification) {
        vxapplication_willResignActive()

        let displayContext = DisplayContext.shared
        displayContext.appIsActive = false
        displayContext.isMouseHidden = false

        NSCursor.unhide() // safety measure, but should be ok
    }
    
    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
        
        // Due to a possible bug, this delegate function is not called.
        // So the VXApplication::willTerminate() call has been moved to the
        // WindowDelegate below, given that this application cannot have multiple
        // windows.
        // vxapplication_willTerminate();
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        // tells the app to quit if the (only) window is closed
        return true
    }
    
    ///
    private func getWindowBackingScaleFactor() -> CGFloat {
        return self.window.backingScaleFactor
    }

    func _selectAll() {
        if let window = textView.window, window.firstResponder == textView {
            textView.selectAll(nil)
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.selectAll(nil)
        }
    }

    @IBAction func selectAll(sender: NSMenuItem) {
        _selectAll()
    }

    func _copy() {
        if let window = textView.window, window.firstResponder == textView {
            textView.copy(nil)
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.copy(nil)
        }
    }

    @IBAction func copy(sender: NSMenuItem) {
        _copy()
    }

    func _paste() {
        if let window = textView.window, window.firstResponder == textView {
            textView.paste(nil)
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.paste(nil)
        }
    }

    @IBAction func paste(sender: NSMenuItem) {
        _paste()
    }

    func _cut() {
        if let window = textView.window, window.firstResponder == textView {
            textView.cut(nil)
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.cut(nil)
        }
    }

    @IBAction func cut(sender: NSMenuItem) {
        _cut()
    }

    func _undo() {
        if let window = textView.window, window.firstResponder == textView {
            textView.undoManager?.undo()
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.undoManager?.undo()
        }
    }

    @IBAction func undo(sender: NSMenuItem) {
        _undo()
    }

    func _redo() {
        if let window = textView.window, window.firstResponder == textView {
            textView.undoManager?.redo()
        }
        if let window = textField.window, let editor = textField.currentEditor(), window.firstResponder == editor {
            editor.undoManager?.redo()
        }
    }

    @IBAction func redo(sender: NSMenuItem) {
        _redo()
    }

    @IBAction func fullScreen(sender: NSMenuItem) {
        window.toggleFullScreen(sender)
    }
    
    // Useful trigger to avoid alert sounds when the first responder can't
    // do anything.
    @IBAction func dontDoAnything(sender: NSMenuItem) {
        // nothing
    }

    // called when the app is opened with a HTTP(S) URL (universal link)
    func application(_ application: NSApplication,
                     continue userActivity: NSUserActivity,
                     restorationHandler: @escaping ([NSUserActivityRestoring]) -> Void) -> Bool
    {
        // Get URL components from the incoming user activity.
        guard userActivity.activityType == NSUserActivityTypeBrowsingWeb,
              let incomingURL = userActivity.webpageURL,
              let _ /*components*/ = NSURLComponents(url: incomingURL, resolvingAgainstBaseURL: true) else {
            return false
        }

        return vxapplication_openURL(incomingURL.absoluteString)
    }

    func application(_ application: NSApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
        let tokenParts = deviceToken.map { data in String(format: "%02.2hhx", data) }
        let token = tokenParts.joined()
        print("🌟 Device Token for Push Notifs: \(token)")
        // notify VXApplication
        vxapplication_didRegisterForRemoteNotifications(pushTokenTypeApple, token);
    }

    func application(_ application: NSApplication, didFailToRegisterForRemoteNotificationsWithError error: Error) {
        // token couldn't be obtained
        // TODO: retry?
    }

    func application(_ application: NSApplication, didReceiveRemoteNotification userInfo: [String : Any]) {
        var title = ""
        var body = ""
        var category = ""
        var badge = 0

        // Access the 'aps' dictionary, which contains standard push notification fields
        if let aps = userInfo["aps"] as? [String: Any] {
            // Extract alert information
            if let alert = aps["alert"] as? [String: Any] {
                title = alert["title"] as? String ?? "No Title"
                body = alert["body"] as? String ?? "No Body"
            }

            // Extract badge number
            if let badgeCount = aps["badge"] as? Int {
                badge = badgeCount
            }
        }

        // Access custom data fields
        if let c = userInfo["category"] as? String {
            category = c
        }

        vxapplication_didReceiveNotification(title.cString(using: .utf8), body.cString(using: .utf8), category, UInt32(badge));
    }
}

extension AppDelegate: NSWindowDelegate {
    
    func windowWillResize(_ sender: NSWindow, to frameSize: NSSize) -> NSSize {
        let displayContext = DisplayContext.shared
        displayContext.windowResizing = true
        displayContext.doneResizingTimer = AppDelegate.DONE_RESIZING_OR_MOVING_TIME

        return NSSize(width: max(frameSize.width, AppDelegate.WINDOWS_MINIMUM_WIDTH),
                      height: max(frameSize.height, AppDelegate.WINDOWS_MINIMUM_HEIGHT))
    }
    
    func windowWillMove(_ notification: Notification) {
        let displayContext = DisplayContext.shared
        displayContext.windowMoving = true
    }
    
    // didMove is called many times, while moving
    // thus before the mouse is released.
    // We can't use just this to detect when the window
    // is really done moving.
    // func windowDidMove(_ notification: Notification) {
    // }
    
    // Called many times, while resizing.
    // It's not a good idea to adapt rendering here.
    // Using doneResizingTimer for that
    //    func windowDidResize(_ notification: Notification) {
    //    }
    
    func windowWillClose(_ notification: Notification) {
        stopDisplayLink();
        vxapplication_willTerminate()
    }
}

extension AppDelegate: NSTextViewDelegate {

    func textViewDidChangeSelection(_ notification: Notification) {
        guard let textView = notification.object as? CustomTextView else {
            return
        }
        if textView.isDidChangeTurnedOff { return }

        guard let gcaRange = getGraphemeClusterAwareRange(textView: textView) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: textView.string, start: gcaRange.start, end: gcaRange.end) else { return }

        // print("\(selectedRange.lowerBound)-\(selectedRange.upperBound) -> \(s.start)-\(s.end)")

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            // hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
            hostPlatformTextInputUpdate(nil, 0, s.start, s.end)
        }
    }

    func textDidChange(_ notification: Notification) {
        guard let textView = notification.object as? CustomTextView else {
            return
        }
        if textView.isDidChangeTurnedOff { return }

        guard let gcaRange = getGraphemeClusterAwareRange(textView: textView) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: textView.string, start: gcaRange.start, end: gcaRange.end) else { return }

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
        }
    }

    func textDidBeginEditing(_ notification: Notification) {
        // nothing to do
    }

    func textDidEndEditing(_ notification: Notification) {
        hostPlatformTextInputClose()
    }
}

class CustomTextField: NSTextField {
    var isDidChangeTurnedOff: Bool = false
    override var allowsVibrancy: Bool {
        return false
    }

    override func becomeFirstResponder() -> Bool {
        let becameFirstResponder = super.becomeFirstResponder()
        if becameFirstResponder, let textView = self.window?.fieldEditor(true, for: self) as? NSTextView {
            // Disable automatic text correction features
            textView.isAutomaticTextReplacementEnabled = false
            textView.isAutomaticSpellingCorrectionEnabled = false
            textView.isAutomaticDataDetectionEnabled = false
            textView.isAutomaticLinkDetectionEnabled = false
            textView.isAutomaticTextCompletionEnabled = false
            textView.isAutomaticDashSubstitutionEnabled = false
            textView.isAutomaticQuoteSubstitutionEnabled = false
            textView.isContinuousSpellCheckingEnabled = false
        }
        return becameFirstResponder
    }

    override func textDidChange(_ notification: Notification) {
        super.textDidChange(notification)

        if isDidChangeTurnedOff { return }

        guard let gcaRange = getTextFieldGraphemeClusterAwareRange(textField: self) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: self.stringValue, start: gcaRange.start, end: gcaRange.end) else { return }

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
        }
    }

    override func textDidEndEditing(_ notification: Notification) {
        super.textDidEndEditing(notification)
        hostPlatformTextInputClose()
    }
}

extension CustomTextField: NSTextViewDelegate {
    func textViewDidChangeSelection(_ notification: Notification) {
        if isDidChangeTurnedOff { return }

        guard let gcaRange = getTextFieldGraphemeClusterAwareRange(textField: self) else { return }

        guard let s = stringAndRangeToUtf8DataAndByteRange(string: self.stringValue, start: gcaRange.start, end: gcaRange.end) else { return }

        s.data.withUnsafeBytes { unsafeRawBufferPointer in
            guard let pointer = unsafeRawBufferPointer.baseAddress?.assumingMemoryBound(to: CChar.self) else {
                return
            }
            hostPlatformTextInputUpdate(pointer, s.data.count, s.start, s.end)
        }
    }
}

class CustomTextView: NSTextView {
    var isDidChangeTurnedOff: Bool = false
    override var allowsVibrancy: Bool {
        return false
    }

}

func getGraphemeClusterAwareRange(textView: NSTextView) -> (start: Int, end: Int)? {
    // Get the NSRange from the NSTextView
    let nsRange = textView.selectedRange()

    // Convert NSRange to Swift String range
    guard let range = Range(nsRange, in: textView.string) else {
        return nil
    }

    // Convert String.Index to Int
    let start = textView.string.distance(from: textView.string.startIndex, to: range.lowerBound)
    let end = textView.string.distance(from: textView.string.startIndex, to: range.upperBound)

    return (start, end)
}

func setGraphemeClusterAwareRange(in textView: NSTextView, string: String, start: Int, end: Int) -> Bool {
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
    textView.setSelectedRange(range)

    return true
}

func getTextFieldGraphemeClusterAwareRange(textField: NSTextField) -> (start: Int, end: Int)? {
    // Get the NSRange from the NSTextView
    guard let text = textField.currentEditor() else { return nil }
    let nsRange = text.selectedRange

    // Convert NSRange to Swift String range
    guard let range = Range(nsRange, in: text.string) else {
        return nil
    }

    // Convert String.Index to Int
    let start = text.string.distance(from: text.string.startIndex, to: range.lowerBound)
    let end = text.string.distance(from: text.string.startIndex, to: range.upperBound)

    return (start, end)
}

func setTextFieldGraphemeClusterAwareRange(textField: NSTextField, string: String, start: Int, end: Int) -> Bool {
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
    if let fieldEditor = textField.window?.fieldEditor(true, for: textField) as? NSTextView {
        // Set the selected range on the field editor
        fieldEditor.setSelectedRange(range)
    }
    return true
}
