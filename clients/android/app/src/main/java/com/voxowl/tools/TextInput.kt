package com.voxowl.tools

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context.CLIPBOARD_SERVICE
import android.util.Log
import androidx.appcompat.widget.AppCompatEditText
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import com.voxowl.blip.MainActivity

class TextInput {

    companion object {
        // @JvmStatic annotations are needed to be able to call the functions from JNI code.

        enum class ReturnKeyType {
            Default,
            Next,
            Done,
            Send
        }

        enum class KeyboardType {
            Default,
            Email,
            Phone,
            OneTimeCode,
            Numbers,
            URL,
            ASCII
        }

        enum class Action {
            Close,
            Copy,
            Paste,
            Cut,
            Undo,
            Redo
        }

        @JvmStatic
        fun textinputRequest(strBytes: ByteArray, strDidChange: Boolean, cursorStart: Long, cursorEnd: Long, multiline: Boolean, keyboardType: Int, returnKeyType: Int) {
            // Log.e("Cubzh", "⭐️[INPUT] REQUEST C++ -> JAVA >> ${String(strBytes, Charsets.UTF_8)}");
            //Log.e("INPUT", "textinputRequest [$multiline] [$keyboardType] [$returnKeyType]")

            // Multiline
            // when API 29 is available, we can use text.isSingleLine
            // val newMaxLines = if (multiline) Int.MAX_VALUE else 1

            // Show keyboard in App's UI thread
            MainActivity.shared?.let {
                it.runOnUiThread {
                    // editText.disableTextChangedListeners()
                    it._mainEditText.tag = true
                    it._mainEditText.update(strBytes, cursorStart.toInt(), cursorEnd.toInt(), multiline, keyboardType, returnKeyType)

                    if (it._mainEditText.requestFocus()) {
                        // val imm = activity.getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
                        // imm.showSoftInput(editText, InputMethodManager.SHOW_IMPLICIT)
                        WindowCompat.getInsetsController(it.window, it._mainEditText)
                            .show(WindowInsetsCompat.Type.ime())
                    }
                    // editText.enableTextChangedListeners()
                    it._mainEditText.tag = null
                }
            }
        }

        @JvmStatic
        fun textinputUpdate(strBytes: ByteArray, strDidChange: Boolean, cursorStart: Long, cursorEnd: Long) {
            // Log.e("Cubzh", "⭐️[INPUT] UPDATE C++ -> JAVA >> $strBytes");
            // Log.e("INPUT", "textinputUpdate $strBytes $strDidChange $cursorStart $cursorEnd")

            MainActivity.shared?.let {
                it.runOnUiThread {
                    // editText.disableTextChangedListeners()
                    it._mainEditText.tag = true
                    it._mainEditText.update(strBytes, strDidChange, cursorStart.toInt(), cursorEnd.toInt())
                    // editText.enableTextChangedListeners()
                    it._mainEditText.tag = null
                }
            }
        }

        @JvmStatic
        fun textinputAction(action: Int) {
            // returns if activity is null
            val activity: MainActivity = MainActivity.shared ?: return
            val editText: AppCompatEditText = activity._mainEditText

            activity.runOnUiThread {
                when (action) {
                    Action.Close.ordinal -> {
                        if (activity.window.decorView.requestFocus()) { // this should un-focus the EditText
                            WindowCompat.getInsetsController(activity.window, editText)
                                .hide(WindowInsetsCompat.Type.ime())
                        }
                    }

                    Action.Copy.ordinal -> {
                        editText.text?.let {
                            // Get the selected text from the EditText
                            val selectedText = it.subSequence(editText.selectionStart, editText.selectionEnd)
                            // Get the ClipboardManager instance
                            val clipboard = activity.getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
                            // Create a ClipData with the selected text
                            val clip = ClipData.newPlainText("label", selectedText)
                            // Set the ClipData to the clipboard
                            clipboard.setPrimaryClip(clip)
                        }
                    }

                    Action.Paste.ordinal -> {
                        editText.text?.let {
                            val clipboard = activity.getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
                            val clipData = clipboard.primaryClip
                            if (clipData != null && clipData.itemCount > 0) {
                                val pasteData = clipData.getItemAt(0).text
                                it.replaceRange(editText.selectionStart, editText.selectionEnd, pasteData)
                            }
                        }
                    }

                    Action.Cut.ordinal -> {
                        editText.text?.let {
                            // Get the selected text from the EditText
                            val selectedText = it.subSequence(editText.selectionStart, editText.selectionEnd)
                            // Get the ClipboardManager instance
                            val clipboard = activity.getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
                            // Create a ClipData with the selected text
                            val clip = ClipData.newPlainText("label", selectedText)
                            // Set the ClipData to the clipboard
                            clipboard.setPrimaryClip(clip)
                            // Delete the selected text from the EditText
                            it.delete(editText.selectionStart, editText.selectionEnd)
                        }
                    }

                    Action.Undo.ordinal -> {
                        // TODO: !
                    }

                    Action.Redo.ordinal -> {
                        // TODO: !
                    }
                }
            }
        }

        // String to UTF8 Bytes
        fun convertSelectionFromStringToUTF8Bytes(text: String, cursorStart: Int, cursorEnd: Int): Pair<Int, Int> {
            // Check if the indices are valid
            require(cursorStart >= 0 && cursorEnd <= text.length && cursorStart <= cursorEnd) {
                "Invalid indices"
            }

            val substrBeforeSelectionStart = text.substring(0, cursorStart)
            val utf8BytesBeforeStart = substrBeforeSelectionStart.toByteArray(Charsets.UTF_8)
            if (cursorStart == cursorEnd) {
                return Pair(utf8BytesBeforeStart.size, utf8BytesBeforeStart.size)
            }
            val substrSelection = text.substring(cursorStart, cursorEnd)
            val utf8BytesSelection = substrSelection.toByteArray(Charsets.UTF_8)
            return Pair(utf8BytesBeforeStart.size, utf8BytesBeforeStart.size+utf8BytesSelection.size)
        }

        // UTF8 Bytes to String
        fun convertSelectionFromUTF8BytesToString(utf8Bytes: ByteArray, cursorStart: Int, cursorEnd: Int): Pair<Int, Int> {
            // Check if the indices are valid
            if (cursorStart < 0 || cursorEnd > utf8Bytes.size || cursorStart > cursorEnd) {
                Log.e("Cubzh Java", "[INPUT] wrong indices")
            }
            require(cursorStart >= 0 && cursorEnd <= utf8Bytes.size && cursorStart <= cursorEnd) {
                "Invalid indices"
            }

            val bytesBeforeStart = utf8Bytes.copyOfRange(0, cursorStart)
            val strBeforeStart = String(bytesBeforeStart, Charsets.UTF_8)

            if (cursorStart == cursorEnd) {
                return Pair(strBeforeStart.length, strBeforeStart.length)
            }

            val bytesBetweenStartAndEnd = utf8Bytes.copyOfRange(cursorStart, cursorEnd)
            val strBetweenStartAndEnd = String(bytesBetweenStartAndEnd, Charsets.UTF_8)

            return Pair(strBeforeStart.length, strBeforeStart.length + strBetweenStartAndEnd.length)
        }
    }

}