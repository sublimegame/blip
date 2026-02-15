//
//  Inputs.swift
//  macos
//
//  Created by Adrien Duermael on 2/27/20.
//

import Cocoa
import Carbon.HIToolbox

func translateModifiers(flags: Int32) -> UInt8 {
    return 0
        | ((0 != (flags & NX_DEVICELSHIFTKEYMASK)) ? UInt8(ModifierShift.rawValue) : 0)
        | ((0 != (flags & NX_DEVICERSHIFTKEYMASK)) ? UInt8(ModifierShift.rawValue) : 0)
        | ((0 != (flags & NX_DEVICELALTKEYMASK)) ? UInt8(ModifierAlt.rawValue) : 0)
        | ((0 != (flags & NX_DEVICERALTKEYMASK)) ? UInt8(ModifierAlt.rawValue) : 0)
        | ((0 != (flags & NX_DEVICELCTLKEYMASK)) ? UInt8(ModifierCtrl.rawValue) : 0)
        | ((0 != (flags & NX_DEVICERCTLKEYMASK)) ? UInt8(ModifierCtrl.rawValue) : 0)
        | ((0 != (flags & NX_DEVICELCMDKEYMASK)) ? UInt8(ModifierSuper.rawValue) : 0)
        | ((0 != (flags & NX_DEVICERCMDKEYMASK)) ? UInt8(ModifierSuper.rawValue) : 0)
}

var translatedKeys = [Int: Input]()

func handleKeyEvent(e: NSEvent) -> (input: Input, characters: String?, modifiers: UInt8) {

    // https://developer.apple.com/documentation/appkit/nsevent/specialkey
    // https://stackoverflow.com/a/16125341

    // init translatedKeys if empty
    if translatedKeys.isEmpty {

        // INDEPENDENT OF KEYBOARD LAYOUT
        translatedKeys[kVK_Return] = InputReturn
        translatedKeys[kVK_Tab] = InputTab
        translatedKeys[kVK_Space] = InputSpace
        translatedKeys[kVK_Delete] = InputBackspace
        translatedKeys[kVK_ForwardDelete] = InputDelete
        translatedKeys[kVK_Escape] = InputEsc
        // translatedKeys[kVK_Command] // MODIFIER, down/up events ignored currently
        translatedKeys[kVK_Shift] = InputShift // MODIFIER
        // translatedKeys[kVK_CapsLock] // MODIFIER, down/up events ignored currently
        translatedKeys[kVK_Option] = InputOption // MODIFIER
        translatedKeys[kVK_Control] = InputControl // MODIFIER
        translatedKeys[kVK_RightShift] = InputRightShift // MODIFIER
        translatedKeys[kVK_RightOption] = InputRightOption // MODIFIER
        translatedKeys[kVK_RightControl] = InputRightControl // MODIFIER
        // translatedKeys[kVK_Function] // MODIFIER, down/up events ignored currently
        // translatedKeys[kVK_VolumeUp] // NOT USED, down/up events ignored currently
        // translatedKeys[kVK_VolumeDown] // NOT USED, down/up events ignored currently
        // translatedKeys[kVK_Mute] // NOT USED, down/up events ignored currently
        translatedKeys[kVK_F1] = InputF1
        translatedKeys[kVK_F2] = InputF2
        translatedKeys[kVK_F3] = InputF3
        translatedKeys[kVK_F4] = InputF4
        translatedKeys[kVK_F5] = InputF5
        translatedKeys[kVK_F6] = InputF6
        translatedKeys[kVK_F7] = InputF7
        translatedKeys[kVK_F8] = InputF8
        translatedKeys[kVK_F9] = InputF9
        translatedKeys[kVK_F10] = InputF10
        translatedKeys[kVK_F11] = InputF11
        translatedKeys[kVK_F12] = InputF12
        translatedKeys[kVK_F13] = InputF13
        translatedKeys[kVK_F14] = InputF14
        translatedKeys[kVK_F15] = InputF15
        translatedKeys[kVK_F16] = InputF16
        translatedKeys[kVK_F17] = InputF17
        translatedKeys[kVK_F18] = InputF18
        translatedKeys[kVK_F19] = InputF19
        translatedKeys[kVK_F20] = InputF20
        // translatedKeys[kVK_Help] // NOT USED
        translatedKeys[kVK_Home] = InputHome
        translatedKeys[kVK_PageUp] = InputPageUp
        translatedKeys[kVK_PageDown] = InputPageDown
        translatedKeys[kVK_End] = InputEnd
        // translatedKeys[kVK_ForwardDelete] // NOT USED
        translatedKeys[kVK_LeftArrow] = InputLeft
        translatedKeys[kVK_RightArrow] = InputRight
        translatedKeys[kVK_DownArrow] = InputDown
        translatedKeys[kVK_UpArrow] = InputUp

        translatedKeys[kVK_ISO_Section] = InputISOSection
        
        // DEPENDENT OF KEYBOARD LAYOUT
        // TODO
        translatedKeys[kVK_ANSI_A] = InputKeyA
        translatedKeys[kVK_ANSI_B] = InputKeyB
        translatedKeys[kVK_ANSI_C] = InputKeyC
        translatedKeys[kVK_ANSI_D] = InputKeyD
        translatedKeys[kVK_ANSI_E] = InputKeyE
        translatedKeys[kVK_ANSI_F] = InputKeyF
        translatedKeys[kVK_ANSI_G] = InputKeyG
        translatedKeys[kVK_ANSI_H] = InputKeyH
        translatedKeys[kVK_ANSI_I] = InputKeyI
        translatedKeys[kVK_ANSI_J] = InputKeyJ
        translatedKeys[kVK_ANSI_K] = InputKeyK
        translatedKeys[kVK_ANSI_L] = InputKeyL
        translatedKeys[kVK_ANSI_M] = InputKeyM
        translatedKeys[kVK_ANSI_N] = InputKeyN
        translatedKeys[kVK_ANSI_O] = InputKeyO
        translatedKeys[kVK_ANSI_P] = InputKeyP
        translatedKeys[kVK_ANSI_Q] = InputKeyQ
        translatedKeys[kVK_ANSI_R] = InputKeyR
        translatedKeys[kVK_ANSI_S] = InputKeyS
        translatedKeys[kVK_ANSI_T] = InputKeyT
        translatedKeys[kVK_ANSI_U] = InputKeyU
        translatedKeys[kVK_ANSI_V] = InputKeyV
        translatedKeys[kVK_ANSI_W] = InputKeyW
        translatedKeys[kVK_ANSI_X] = InputKeyX
        translatedKeys[kVK_ANSI_Y] = InputKeyY
        translatedKeys[kVK_ANSI_Z] = InputKeyZ
        
        // number keys above letters
        translatedKeys[kVK_ANSI_0] = InputKey0
        translatedKeys[kVK_ANSI_1] = InputKey1
        translatedKeys[kVK_ANSI_2] = InputKey2
        translatedKeys[kVK_ANSI_3] = InputKey3
        translatedKeys[kVK_ANSI_4] = InputKey4
        translatedKeys[kVK_ANSI_5] = InputKey5
        translatedKeys[kVK_ANSI_6] = InputKey6
        translatedKeys[kVK_ANSI_7] = InputKey7
        translatedKeys[kVK_ANSI_8] = InputKey8
        translatedKeys[kVK_ANSI_9] = InputKey9
        
        // side keypad
        translatedKeys[kVK_ANSI_Keypad0] = InputNumPad0
        translatedKeys[kVK_ANSI_Keypad1] = InputNumPad1
        translatedKeys[kVK_ANSI_Keypad2] = InputNumPad2
        translatedKeys[kVK_ANSI_Keypad3] = InputNumPad3
        translatedKeys[kVK_ANSI_Keypad4] = InputNumPad4
        translatedKeys[kVK_ANSI_Keypad5] = InputNumPad5
        translatedKeys[kVK_ANSI_Keypad6] = InputNumPad6
        translatedKeys[kVK_ANSI_Keypad7] = InputNumPad7
        translatedKeys[kVK_ANSI_Keypad8] = InputNumPad8
        translatedKeys[kVK_ANSI_Keypad9] = InputNumPad9
        
        translatedKeys[kVK_ANSI_Equal] = InputEqual
        translatedKeys[kVK_ANSI_Minus] = InputMinus
        translatedKeys[kVK_ANSI_KeypadMinus] = InputNumPadMinus
        translatedKeys[kVK_ANSI_KeypadPlus] = InputNumPadPlus
        translatedKeys[kVK_ANSI_KeypadClear] = InputClear
        translatedKeys[kVK_ANSI_KeypadDecimal] = InputDecimal
        translatedKeys[kVK_ANSI_KeypadMultiply] = InputMultiply
        translatedKeys[kVK_ANSI_KeypadDivide] = InputDivide
        translatedKeys[kVK_ANSI_KeypadEquals] = InputNumPadEqual
        translatedKeys[kVK_ANSI_KeypadEnter] = InputReturnKP
        translatedKeys[kVK_ANSI_RightBracket] = InputRightBracket
        translatedKeys[kVK_ANSI_LeftBracket] = InputLeftBracket
        
        translatedKeys[kVK_ANSI_Quote] = InputQuote
        translatedKeys[kVK_ANSI_Semicolon] = InputSemicolon
        translatedKeys[kVK_ANSI_Backslash] = InputBackslash
        translatedKeys[kVK_ANSI_Comma] = InputComma
        translatedKeys[kVK_ANSI_Slash] = InputSlash
        translatedKeys[kVK_ANSI_Period] = InputPeriod
        translatedKeys[kVK_ANSI_Grave] = InputTilde
    }
    
    let keyCode: UInt16 = e.keyCode
    let keyCodeInt: Int = Int(keyCode)
    var input = translatedKeys[keyCodeInt] ?? InputNone
    let modifiers = translateModifiers(flags: Int32(e.modifierFlags.rawValue))
    
    // do not transmit special key chars
    if e.type == .flagsChanged || e.specialKey != nil {
        return (input, nil, modifiers)
    }

    var chars = e.characters

    if modifiers & UInt8(ModifierSuper.rawValue) != 0 {
        if let chars = e.characters {
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
        }
        return (input, nil, modifiers)
    }
    
    if modifiers & UInt8(ModifierCtrl.rawValue) != 0 {
        return (input, nil, modifiers)
    }
    
    // replace chars in some cases, when alt is pressed
    if modifiers & UInt8(ModifierAlt.rawValue) != 0 {
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
    
    return (input, chars, modifiers)
}
