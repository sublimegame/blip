package com.voxowl.blip

import android.content.Context
import android.os.Build
import android.text.Editable
import android.text.InputFilter
import android.text.NoCopySpan
import android.text.Selection
import android.text.Spanned
import android.text.TextWatcher
import android.util.AttributeSet
import android.util.Log
import android.view.inputmethod.EditorInfo
import androidx.appcompat.widget.AppCompatEditText
import com.voxowl.tools.TextInput
import java.util.regex.Pattern

class ASCIIInputFilter : InputFilter {

    // ASCII
    private val mPattern: Pattern = Pattern.compile("[\\x00-\\x7F]+")

    override fun filter(
        source: CharSequence?,
        start: Int,
        end: Int,
        dest: Spanned?,
        dstart: Int,
        dend: Int
    ): CharSequence? {
        val formattedSource = dest?.subSequence(0, dstart).toString()
        val destPrefix = source?.subSequence(start, end).toString()
        val destSuffix = dest?.subSequence(dend, dest.length).toString()
        val match = formattedSource + destPrefix + destSuffix
        val matcher = mPattern.matcher(match)
        return if (!matcher.matches()) "" else null
    }
}

interface VXSelectionChangedWatcher : NoCopySpan {
    fun onSelectionChanged(selStart: Int, selEnd: Int);
}

class VXEditText : AppCompatEditText {

    constructor(context: Context) : super(context)
    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)
    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    // Collection of selection changed watchers
    private var mSelectionWatchers: ArrayList<VXSelectionChangedWatcher>? = null

    //
    // var isDidChangeDisabled: Boolean = false

    private var mTextWatchers: ArrayList<TextWatcher>? = null
    override fun addTextChangedListener(watcher: TextWatcher) {
        if (mTextWatchers == null) {
            mTextWatchers = ArrayList()
        }
        mTextWatchers?.add(watcher)
        super.addTextChangedListener(watcher)
    }

    override fun removeTextChangedListener(watcher: TextWatcher) {
        mTextWatchers?.let {
            val i: Int = it.indexOf(watcher)
            if (i >= 0) {
                it.removeAt(i)
            }
        }
        super.removeTextChangedListener(watcher)
    }

    fun disableTextChangedListeners() {
        val textWatchers = mTextWatchers?: return
        for (tw in textWatchers) {
            super.removeTextChangedListener(tw)
        }
    }

    fun enableTextChangedListeners() {
        val textWatchers = mTextWatchers?: return
        for (tw in textWatchers) {
            super.addTextChangedListener(tw)
        }
    }

    fun update(strUTF8Bytes: ByteArray, cursorStartInBytes: Int, cursorEndInBytes: Int, multiline: Boolean, keyboardType: Int, returnKeyType: Int) {

        // validate arguments
        if (multiline && keyboardType != TextInput.Companion.KeyboardType.Default.ordinal) {
            Log.e("Cubzh Java", "Update of VXEditText rejected. Multiline requires keyboard type to be Default.")
            return
        }
        if (cursorStartInBytes < 0 || cursorStartInBytes > strUTF8Bytes.size) {
            Log.e("Cubzh Java", "Update of VXEditText rejected. CursorStart cannot be out of bounds.")
            return
        }
        if (cursorEndInBytes < 0 || cursorEndInBytes > strUTF8Bytes.size) {
            Log.e("Cubzh Java", "Update of VXEditText rejected. CursorEnd cannot be out of bounds.")
            return
        }

        // Conversions
        val str = String(strUTF8Bytes, Charsets.UTF_8)
        val cursor = TextInput.convertSelectionFromUTF8BytesToString(strUTF8Bytes, cursorStartInBytes, cursorEndInBytes)
        val cursorStart = cursor.first
        val cursorEnd = cursor.second

        // Input type + multiline
        if (multiline) {
            // TextInput.Companion.KeyboardType.Default can be assumed
            this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_NORMAL or EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE
        } else {
            when (keyboardType) {
                TextInput.Companion.KeyboardType.Default.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_NORMAL
                }
                TextInput.Companion.KeyboardType.Email.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
                }
                TextInput.Companion.KeyboardType.Phone.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_PHONE
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        this.setAutofillHints("phoneNational") // AUTOFILL_HINT_PHONE_NATIONAL
                    }
                }
                TextInput.Companion.KeyboardType.OneTimeCode.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_NUMBER
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        this.setAutofillHints("smsOTPCode") // AUTOFILL_HINT_SMS_OTP
                    }
                }
                TextInput.Companion.KeyboardType.Numbers.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_NUMBER
                }
                TextInput.Companion.KeyboardType.URL.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_URI
                }
                TextInput.Companion.KeyboardType.ASCII.ordinal -> {
                    this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_NORMAL
                }
                else -> {
                    this.inputType = EditorInfo.TYPE_CLASS_TEXT or EditorInfo.TYPE_TEXT_VARIATION_NORMAL
                }
            }
        }

        // TODO: is this needed?
        // editText.maxLines = newMaxLines
        // if (newMaxLines == 1) {
        //     editText.setLines(newMaxLines)
        // }

        // ASCII filtering
        if (keyboardType == TextInput.Companion.KeyboardType.ASCII.ordinal) {
            // force ASCII
            this.filters = arrayOf(ASCIIInputFilter())
        } else {
            // do not force ASCII
            this.filters = arrayOf()
        }

        // Return key type
        this.returnKeyType = returnKeyType

        // Text
        this.text = Editable.Factory.getInstance().newEditable(str)

        // Selection
        this.setSelection(cursorStart, cursorEnd)
    }

    fun update(strUTF8Bytes: ByteArray, strDidChange: Boolean, cursorStartInBytes: Int, cursorEndInBytes: Int) {

        // validate arguments
        if (cursorStartInBytes < 0 || cursorStartInBytes > strUTF8Bytes.size) {
            Log.e("Cubzh Java", "Update of VXEditText rejected. CursorStart cannot be out of bounds.")
            return
        }
        if (cursorEndInBytes < 0 || cursorEndInBytes > strUTF8Bytes.size) {
            Log.e("Cubzh Java", "Update of VXEditText rejected. CursorEnd cannot be out of bounds.")
            return
        }

        // Choose the string to use // TODO: this doesn't always work (race condition?)
        val strUTF8BytesToUse = when(strDidChange) {
            true -> strUTF8Bytes
            false -> this.text.toString().toByteArray()
        }
        // val strUTF8BytesToUse = strUTF8Bytes

        // Conversions
        val str = String(strUTF8BytesToUse, Charsets.UTF_8)
        val cursor = TextInput.convertSelectionFromUTF8BytesToString(strUTF8BytesToUse, cursorStartInBytes, cursorEndInBytes)
        val cursorStart = cursor.first
        val cursorEnd = cursor.second

        // Update text and selection
        if (strDidChange) {
            val newText = Editable.Factory.getInstance().newEditable(str)
            Selection.setSelection(newText, cursorStart, cursorEnd)
            this.text = newText
        } else if (this.selectionStart != cursorStart || this.selectionEnd != cursorEnd) {
            // Update selection
            this.setSelection(cursorStart, cursorEnd)
        }
    }

    var returnKeyType: Int = 0
    set(value) {
        field = value
        when (value) {
            TextInput.Companion.ReturnKeyType.Default.ordinal -> this.setImeOptions(EditorInfo.IME_ACTION_NONE)
            TextInput.Companion.ReturnKeyType.Next.ordinal -> this.setImeOptions(EditorInfo.IME_ACTION_NEXT)
            TextInput.Companion.ReturnKeyType.Done.ordinal -> this.setImeOptions(EditorInfo.IME_ACTION_DONE)
            TextInput.Companion.ReturnKeyType.Send.ordinal -> this.setImeOptions(EditorInfo.IME_ACTION_SEND)
            else -> {
                field = TextInput.Companion.ReturnKeyType.Default.ordinal
                this.setImeOptions(EditorInfo.IME_ACTION_NONE)
            }
        }
    }

    // Called when the selection/cursor changes
    override fun onSelectionChanged(selStart: Int, selEnd: Int) {
        super.onSelectionChanged(selStart, selEnd) // Always call
        // the super implementation, which informs the accessibility
        // subsystem about the selection change.

        mSelectionWatchers?.let {
            for (watcher in it) {
                watcher.onSelectionChanged(selStart, selEnd)
            }
        }
    }

    fun addSelectionChangedListener(watcher: VXSelectionChangedWatcher) {
        if (mSelectionWatchers == null) {
            mSelectionWatchers = ArrayList()
        }
        mSelectionWatchers?.add(watcher)
    }

    fun removeSelectionChangedListener(watcher: VXSelectionChangedWatcher) {
        val watchers = mSelectionWatchers?: return
        val i: Int = watchers.indexOf(watcher)
        if (i >= 0) {
            watchers.removeAt(i)
        }
    }

//    private var _inputConnection: VXInputConnection? = null
//    private var _callback:((Int) -> Unit)? = null
//    override fun onCreateInputConnection(outAttrs: EditorInfo?): InputConnection {
//        val ic = VXInputConnection(super.onCreateInputConnection(outAttrs), true)
//        _inputConnection = ic
//        _inputConnection!!._callback = _callback
//        return ic
//    }

//    fun setKeyCallback(callback: (Int) -> Unit) {
//        // Make sure callback is set, whether onCreateInputConnection is called before or after this
//        _callback = callback
//        _inputConnection?._callback = callback
//    }

//    private class VXInputConnection(target: InputConnection?, mutable: Boolean) : InputConnectionWrapper(target, mutable) {
//        var _callback:((Int) -> Unit)? = null
//
//        override fun sendKeyEvent(event: KeyEvent): Boolean {
//            if (event.action == KeyEvent.ACTION_DOWN) {
//                _callback?.let {
//                    it. invoke(event.keyCode)
//                    return false // consume event
//                }
//            }
//            return super.sendKeyEvent(event)
//        }
//
//        // On some keyboards, deleteSurroundingText(1, 0) may be called for backspace,
//        // can be tested eg. on the meanIT X6
//        /*override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
//            return if (beforeLength == 1 && afterLength == 0) {
//                (sendKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL))
//                        && sendKeyEvent(KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL)))
//            } else super.deleteSurroundingText(beforeLength, afterLength)
//        }*/
//    }
}
