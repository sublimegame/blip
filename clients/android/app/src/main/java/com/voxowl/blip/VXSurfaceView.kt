package com.voxowl.blip

import android.content.Context
import android.renderscript.Float2
import android.text.InputType
import android.util.AttributeSet
import android.util.Log
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import androidx.core.view.OnApplyWindowInsetsListener
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

class VXSurfaceView(context: Context, attrs: AttributeSet) : SurfaceView(context, attrs), OnApplyWindowInsetsListener {
    private var _didFinishLaunching: ((surface: Surface, width: Int, height: Int,
                                       topInset: Float, bottomInset: Float,
                                       leftInset: Float, rightInset: Float,
                                       pixelDensity: Float)->Unit)? = null
    private var _onTick: ((dt: Double)->Unit)? = null
    private var _didResize: ((width: Int, height: Int, pixelsPerPoint: Float,
                              topInset: Float, bottomInset: Float,
                              leftInset: Float, rightInset: Float)->Unit)? = null
    private var _postTouchEvent: ((id: Int, x: Float, y: Float, dx: Float, dy: Float,
                                   state: Int, move: Boolean)->Unit)? = null
    private var _willTerminate: (()->Unit)? = null
    private var _currentWidth: Int = 0
    private var _currentHeight: Int = 0
    private var _touchPosMap: MutableMap<Int, Float2> = mutableMapOf()
    private var _topInset: Float = 0f
    private var _bottomInset: Float = 0f
    private var _leftInset: Float = 0f
    private var _rightInset: Float = 0f
    private var _actionBarShown: Boolean = false

    private val surfaceCallback: SurfaceHolder.Callback = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            didFinishLaunchingInternal()
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            didResizeInternal(width, height, false)
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            _willTerminate?.invoke()
        }
    }

    init {
        val metrics = resources.displayMetrics
        Log.d("VXSurfaceView", "Screen physical dpi: ${metrics.xdpi}x${metrics.ydpi}")

        ViewCompat.setOnApplyWindowInsetsListener(this, this)

        this.isFocusable = true
        this.isFocusableInTouchMode = true

        holder.addCallback(surfaceCallback)
    }

    // We used this:
    // https://stackoverflow.com/questions/58718736/how-to-disable-autocomplete-suggest-when-using-inputmethodmanager-showsoftkeyboa/58769429
    override fun onCreateInputConnection(outAttrs: EditorInfo?): InputConnection {
        if (outAttrs != null) {
            outAttrs.imeOptions = outAttrs.imeOptions or EditorInfo.IME_FLAG_NO_EXTRACT_UI
            outAttrs.inputType = outAttrs.inputType or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
        }
        return BaseInputConnection(this, false)
    }

    private fun didFinishLaunchingInternal() {
        _didFinishLaunching?.let {
            val displayDensity: Float = resources.displayMetrics.density

            it.invoke(this.holder.surface, this.width, this.height,
                _topInset / displayDensity,
                _bottomInset / displayDensity,
                _leftInset / displayDensity,
                _rightInset / displayDensity,
                displayDensity)
        }
    }

    private fun didResizeInternal(width: Int, height: Int, forceResize: Boolean) {
        _didResize?.let {
            val displayDensity: Float = resources.displayMetrics.density

            if (forceResize || _currentWidth != width || _currentHeight != height) {
                _currentWidth = width
                _currentHeight = height

                it.invoke(width, height, displayDensity,
                    _topInset / displayDensity,
                    _bottomInset / displayDensity,
                    _leftInset / displayDensity,
                    _rightInset / displayDensity)
            }
        }
    }

    private fun postTouchEventToC(touchId: Int, x: Float, y: Float, state: Int, move: Boolean, remove: Boolean) {
        val yBottom = _currentHeight - y // We use bottom-left screen coord
        val delta = _touchPosMap[touchId]?.let { Float2 (x - it.x, yBottom - it.y) }
            ?: Float2 (0f, 0f)

        if (remove) {
            _touchPosMap.remove(touchId)
            _postTouchEvent?.invoke(touchId, x, yBottom, delta.x, delta.y, state, move)
        } else if (_touchPosMap.count() < 2 || _touchPosMap.containsKey(touchId)) {
            _touchPosMap[touchId] = Float2(x, yBottom)
            _postTouchEvent?.invoke(touchId, x, yBottom, delta.x, delta.y, state, move)
        }
    }

    override fun onTouchEvent(event: MotionEvent?): Boolean {
        event?.let { me ->

            val actionMasked = me.actionMasked
            val pointerIndex = me.actionIndex

            if (actionMasked == MotionEvent.ACTION_MOVE) {

                for (i in 0 until me.pointerCount) {
                    postTouchEventToC(me.getPointerId(i), me.getX(i), me.getY(i), state = 0, move = true, remove = false);
                }

            } else {

                val pointerId = me.getPointerId(pointerIndex)
                val x = me.getX(pointerIndex)
                val y = me.getY(pointerIndex)
                var state = 0
                var move = false
                var remove = false
                var skipped = false

                when(actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        state = 1
                        move = false
                    }
                    MotionEvent.ACTION_POINTER_DOWN -> {
                        state = 1
                        move = false
                    }
                    MotionEvent.ACTION_POINTER_UP -> {
                        state = 2
                        move = false
                        remove = true
                    }
                    MotionEvent.ACTION_UP -> {
                        state = 2
                        move = false
                        remove = true
                    }
                    MotionEvent.ACTION_CANCEL -> {
                        state = 3
                        move = false
                        remove = true
                    }
                    else -> {
                        Log.d("VXSurfaceView", "touch skipped")
                        skipped = true
                    }
                }

                if (!skipped) {
                    postTouchEventToC(pointerId, x, y, state, move, remove)
                }
            }
        }

        // return true so that motion is never interrupted for capturing raw motion inputs
        return true
    }

    override fun onApplyWindowInsets(v: View, insets: WindowInsetsCompat): WindowInsetsCompat {
        Log.d("VXSurfaceView", "onApplyWindowInsets")
        if (v != this) {
            Log.e("VXSurfaceView", "[onApplyWindowInsets] This should not happen")
            return WindowInsetsCompat(null)
        }

        val prevTopInset = _topInset
        val prevBottomInset = _bottomInset
        val prevLeftInset = _leftInset
        val prevRightInset = _rightInset

        // We set our app to be fullscreen, but it is possible to reveal:
        // - overlay title bar & action bar when swiping from the top or bottom of the screen, these
        // are transparent and will automatically hide after a short delay
        // - solid action bar comes up automatically with the keyboard, always on the side opposite
        // to the camera ie. physical bottom of the phone, regardless of orientation
        // It means we need to take into account 2 sources of insets:
        // 1) insets from physical cutouts such as for the Pixel 3 XL
        // 2) insets from solid action bar, WHEN the keyboard is shown
        // /!\ for some reason, insets from solid action bar is always included in system insets
        // even if the action bar isn't visible... so we check explicitly if it is shown
        val cutout = insets.displayCutout
        val systemInsets = if (_actionBarShown) insets.systemWindowInsets else null
        _topInset = ((cutout?.safeInsetTop ?: 0) /*+ (systemInsets?.top ?: 0)*/).toFloat()
        _bottomInset = ((cutout?.safeInsetBottom ?: 0) /*+ (systemInsets?.bottom ?: 0)*/).toFloat()
        _leftInset = ((cutout?.safeInsetLeft ?: 0) /*+ (systemInsets?.left ?: 0)*/).toFloat()
        _rightInset = ((cutout?.safeInsetRight ?: 0) /*+ (systemInsets?.right ?: 0)*/).toFloat()

        // force resize if insets have changed but width/height did not (eg. closing keyboard)
        if (prevTopInset != _topInset || prevBottomInset != _bottomInset || prevLeftInset != _leftInset || prevRightInset != _rightInset) {
            didResizeInternal(_currentWidth, _currentHeight, true)
        }

        return insets
    }

    fun setListeners(didFinishLaunching: (surface: Surface, width: Int, height: Int,
                                          topInset: Float, bottomInset: Float,
                                          leftInset: Float, rightInset: Float,
                                          pixelDensity: Float)->Unit,
                     onTick: (Double)->Unit,
                     didResize: (width: Int, height: Int, pixelsPerPoint: Float,
                                 topInset: Float, bottomInset: Float,
                                 leftInset: Float, rightInset: Float)->Unit,
                     postTouchEvent: (id: Int, x: Float, y: Float, dx: Float, dy: Float,
                                      state: Int, move: Boolean)->Unit,
                     willTerminate: ()->Unit) {

        _didFinishLaunching = didFinishLaunching
        _onTick = onTick
        _didResize = didResize
        _postTouchEvent = postTouchEvent
        _willTerminate = willTerminate
    }

    fun setActionBarShown(value: Boolean) {
        _actionBarShown = value
    }
}
