package com.voxowl.blip

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.ComponentCallbacks2
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.content.res.Resources
import android.graphics.Rect
import android.media.AudioAttributes
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.Surface
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowInsetsCompat
import com.google.firebase.annotations.concurrent.Background
import com.voxowl.tools.Filesystem
import com.voxowl.tools.Notifications
import java.io.FileNotFoundException
import java.lang.ref.WeakReference

// const val INPUT_ACTION_DOWN = 0
// const val INPUT_ACTION_UP = 1

class MainActivity : AppCompatActivity(), ComponentCallbacks2 {
    // This is equivalent of static fields
    companion object {
        lateinit var weakActivity:  WeakReference<MainActivity>
        val shared: MainActivity?
            get() {
                val result = weakActivity.get()
                if (result == null) {
                    Log.e("Cubzh Java", "🔥🔥🔥MainActivity is NULL.")
                }
                return result
            }
    }

    // Public attributes

    // Text inputs
    lateinit var _mainEditText: VXEditText

    // Private attributes

    private val DT_TARGET: Long = 1000/50 // max 50 fps
    private lateinit var _glView: VXSurfaceView
    // Threads
    private lateinit var _cubzhThread: HandlerThread
    private lateinit var _cubzhThreadHandler: Handler
    // public accessor
    fun getCubzhThreadHandler(): Handler {
        return _cubzhThreadHandler
    }

    // private var _sendTextInputEvent: Boolean = false
    private var _paused: Boolean = false
    private var _tickScheduled: Boolean = false

    // Keyboard events
    private var _keyboardHeight: Float = -1.0f
    private var _keyboardOrientation: Int = -1

    //region vx-wrapper
    private external fun init(assets: AssetManager, appStoragePath: String)
    private external fun tick(deltaTime: Double)
    private external fun willTerminate()
    private external fun didBecomeActive()
    private external fun willResignActive()
    private external fun willEnterForeground()
    private external fun didEnterBackground()
    private external fun didReceiveMemoryWarning()
    private external fun postKeyboardInput(charcode: Int, key: Int, modifiers: Int, state: Int)
    private external fun sendAppClassLoaderToC(classLoader: ClassLoader)
    private external fun callbackImportFile(data: ByteArray, statusInt: Int)
    private external fun keyboardDidOpen(width: Float, height: Float, duration: Float)
    private external fun keyboardDidClose(duration: Float)
    private external fun setStartupAppLink(urlStr: String)
    private external fun openAppLink(urlStr: String)
    private external fun textInputUpdate(str: String, cursorStart: Long, cursorEnd: Long)
    private external fun textInputDone()
    private external fun textInputNext()
    private external fun didFinishLaunching(surface: Surface, width: Int, height: Int, topInset: Float,
                                            bottomInset: Float, leftInset: Float, rightInset: Float,
                                            pixelDensity: Float)
    private external fun didResize(width: Int, height: Int, pixelsPerPoint: Float,
                                   topInset: Float, bottomInset: Float,
                                   leftInset: Float, rightInset: Float)
    private external fun postTouchEvent(id: Int, x: Float, y: Float, dx: Float, dy: Float,
                                        state: Int, move: Boolean)
    private external fun render()
    //endregion

    @Background
    private fun createChannel(chanID: String, chanName: String, chanDesc: String, soundID: Int) {
        val sound = Uri.parse("android.resource://" + applicationContext.packageName + "/" + soundID)
        val mChannel: NotificationChannel
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mChannel = NotificationChannel(chanID, chanName, NotificationManager.IMPORTANCE_HIGH)
            // mChannel.lightColor = color
            // mChannel.enableLights(true)
            mChannel.description = chanDesc
            val audioAttributes = AudioAttributes.Builder()
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .setUsage(AudioAttributes.USAGE_ALARM)
                .build()
            mChannel.setSound(sound, audioAttributes)

            val notificationManager = applicationContext.getSystemService(NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.createNotificationChannel(mChannel)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.v("Cubzh Java", "[VXLifecycle] onCreate")
        super.onCreate(savedInstanceState)

        // Create notification channels, so that custom sounds are played
        createChannel("generic", "Push", "", R.raw.notif)
        createChannel("money", "Push (money)", "", R.raw.notif_money)

        // Update the static weak reference
        weakActivity = WeakReference(this)

        // Prevents the device from going to sleep when this activity is displayed
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Support display cutouts (notches) for API level 28 and up
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // display cutouts must be considered
            window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            // by making the status bar translucent, we can render the game there, in the cutout area
            window.addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS)
        }

        //
        Filesystem.init(this.applicationContext, this)
        Notifications.init(this.applicationContext, this)
        InstallReferrerUtil.setContext(this.applicationContext, this)

        // Thread that Cubzh will use as main thread
        _cubzhThread = HandlerThread("CubzhMain")
        _cubzhThread.start()
        _cubzhThreadHandler = Handler(_cubzhThread.looper)

        // Initialize Cubzh
        _cubzhThreadHandler.post {
            InstallReferrerUtil.sendInstallReferrerInfoToCppIfNeeded()
            this.sendAppClassLoaderToC(this.applicationContext.classLoader)

            init(resources.assets, this.applicationContext.getExternalFilesDir(null)?.path.toString())

            // check if the app has been opened with a link
            val appLinkData = intent.data
            if (appLinkData != null && appLinkData.host == "app.cu.bzh") {
                setStartupAppLink(appLinkData.toString())
            }
        }

        setContentView(R.layout.activity_main)

        // Get the root view
        val rootView = findViewById<View>(R.id.activity_layout)
        rootView.viewTreeObserver.addOnGlobalLayoutListener {

            // Calculate the height of the root view
            val screenHeight = rootView.rootView.height

            // Visible screen height
            val rect = Rect()
            rootView.getWindowVisibleDisplayFrame(rect)
            val visibleHeight = rect.height()

            // Keyboard height
            val keyboardHeight: Int = screenHeight - visibleHeight

            // REMIND, you may like to change this using the fullscreen size of the phone
            // and also using the status bar and navigation bar heights of the phone to calculate
            // the keyboard height. But this worked fine on a Nexus.
            val orientation: Int = this.getResources().configuration.orientation

            onKeyboardHeightChanged(keyboardHeight, orientation)
        }

        _glView = this.findViewById(R.id.glSurfaceView)
        _glView.setListeners(::didFinishLaunchingInternal, ::tickInternal, ::didResizeInternal, ::postTouchEventInternal, ::willTerminateInternal)

        // Native EditText used for keyboard inputs,
        // - listen w/ a TextWatcher instead of OnKeyListener as the latter doesn't work for soft keyboards
        // - use custom VXEditText to capture keys/actions that aren't sent downstream on some keyboards
        // (eg. Samsung keyboard for BACKSPACE, SwiftKey for ENTER)
        _mainEditText = this.findViewById(R.id.vxEditText)

        // Watch text changes
        _mainEditText.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}

            override fun afterTextChanged(s: Editable?) {
                // This method is called after the text changes
                if (_mainEditText.tag == null) {
                    // Notify the C/C++ layer
                    _cubzhThreadHandler.post {
                        textInputUpdate(s.toString(), _mainEditText.selectionStart.toLong(), _mainEditText.selectionEnd.toLong())
                    }
                }
            }
        })

//        _mainEditText.addSelectionChangedListener(object : VXSelectionChangedWatcher {
//            override fun onSelectionChanged(selStart: Int, selEnd: Int) {
//                val textStr = _mainEditText.text.toString()
//                val selectionInUTF8Bytes = TextInput.convertSelectionFromStringToUTF8Bytes(textStr, selStart, selEnd)
//                if (_mainEditText.tag == null) {
//                    // Notify the C/C++ layer the cursor has moved
//                    // textInputUpdate(textStr, selectionInUTF8Bytes.first.toLong(), selectionInUTF8Bytes.second.toLong())
//                }
//            }
//        })

        _mainEditText.setOnEditorActionListener { view, actionId, event ->
            if (actionId == EditorInfo.IME_ACTION_DONE || actionId == EditorInfo.IME_ACTION_SEND) {
                _cubzhThreadHandler.post {
                    textInputDone()
                }
                true // consume event
            } else if (actionId == EditorInfo.IME_ACTION_NEXT) {
                _cubzhThreadHandler.post {
                    textInputNext()
                }
                true // consume event
            } else {
                false
            }
        }
        //_mainEditText.setKeyCallback(::sendEditTextKey)

        // OLD
        // setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE or WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE)
        // NEW
        // SOFT_INPUT_STATE_UNCHANGED is used here to avoid the keyboard opening on its own when
        // putting the app to the background and then back to the foreground.
        // (maybe it is not the best SOFT_INPUT_STATE value to use, but in the meantime it improves the app's behavior)
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE or WindowManager.LayoutParams.SOFT_INPUT_STATE_UNCHANGED)

        val activityManager = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val configurationInfo = activityManager.deviceConfigurationInfo
        Log.d("Cubzh Java", "[VXLifecycle] glEsVersion ${configurationInfo.glEsVersion.toDouble()}")
        Log.d("Cubzh Java", "[VXLifecycle] reqGlEsVersion ${configurationInfo.reqGlEsVersion}")
        Log.d("Cubzh Java", "[VXLifecycle] supports AEP ${packageManager.hasSystemFeature(PackageManager.FEATURE_OPENGLES_EXTENSION_PACK)}")
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)

        val appLinkData = intent.data
        if (appLinkData != null) {
            if (appLinkData.scheme == "https" &&
                appLinkData.host == "app.cu.bzh") {
                _cubzhThreadHandler.post {
                    this.openAppLink(appLinkData.toString())
                }
            }
        }
    }

    override fun onStart() {
        Log.v("Cubzh Java", "[VXLifecycle] onStart")
        super.onStart()
    }

    override fun onPause() {
        Log.v("Cubzh Java", "[VXLifecycle] onPause")
        _paused = true

        _cubzhThreadHandler.post {
            didEnterBackground()
        }

        super.onPause()
    }

    override fun onResume() {
        Log.v("Cubzh Java", "[VXLifecycle] onResume")
        super.onResume()

        _cubzhThreadHandler.post {
            willEnterForeground()
        }

        setSystemUIFlags()

        _paused = false
    }

    override fun onStop() {
        Log.v("Cubzh Java", "[VXLifecycle] onStop")

        super.onStop()
    }

    override fun onDestroy() {
        Log.v("Cubzh Java", "[VXLifecycle] onDestroy")

        weakActivity.clear()

        _cubzhThread.quitSafely()

        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, resultIntent: Intent?) {
        super.onActivityResult(requestCode, resultCode, resultIntent)

        if (requestCode == Filesystem.IMPORT_REQUEST_CODE) {
            val statusOk = 0        // ImportFileCallbackStatus::OK
            val statusError = 1     // ImportFileCallbackStatus::ERROR_IMPORT
            val statusCancelled = 2 // ImportFileCallbackStatus::CANCELLED
            var bytes = ByteArray(0)
            var status = statusError

            if (resultCode == RESULT_OK) {
                // if resultIntent and resultIntent.data are both not null,
                // then calls this block with uri as resultIntent.data
                resultIntent?.data?.also { uri ->
                    try {
                        // get the InputStream for the URI, may throw FileNotFoundException
                        val selectedFileInputStream = contentResolver.openInputStream(uri)

                        // read the bytes from the URI if not null
                        bytes = selectedFileInputStream?.readBytes() ?: bytes
                        selectedFileInputStream?.close()

                        if (selectedFileInputStream != null) {
                            // we were able to read the data
                            status = statusOk
                        }

                    } catch (e: FileNotFoundException) {
                        // no data associated with the URI
                        status = statusError
                    }
                }
            } else if (resultCode == RESULT_CANCELED) {
                // file picking has been cancelled
                status = statusCancelled
            }

            // call JNI function
            _cubzhThreadHandler.post {
                callbackImportFile(bytes, status)
            }
        }
    }

    override fun onTrimMemory(level: Int) {
        Log.v("Cubzh Java", "[VXLifecycle] onTrimMemory $level")
        super.onTrimMemory(level)

        when(level) {
            ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN -> { }
            // Device is running low on memory while the app is running
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE,
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW,
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> {
                _cubzhThreadHandler.post {
                    didReceiveMemoryWarning()
                }
            }
            // Device is running low on memory with the app in background and process may be killed
            ComponentCallbacks2.TRIM_MEMORY_BACKGROUND,
            ComponentCallbacks2.TRIM_MEMORY_MODERATE,
            ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> {
                _cubzhThreadHandler.post {
                    didReceiveMemoryWarning()
                }
            }
            // Unrecognized memory level value from system, treat this as generic low-memory message
            else -> {
                _cubzhThreadHandler.post {
                    didReceiveMemoryWarning()
                }
            }
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)

        if (hasFocus) {
            setSystemUIFlags()
            _cubzhThreadHandler.post {
                didBecomeActive()
            }
        } else {
            _cubzhThreadHandler.post {
                willResignActive()
            }
        }
    }

    private fun onKeyboardHeightChanged(height: Int, orientation: Int) {

        if (_keyboardHeight == height.toFloat() && _keyboardOrientation == orientation) {
            // keyboard height and orientation have not changed, abort.
            return
        }

        val (statusBarHeight, navigationBarHeight) = getSystemBarInsets(window.decorView)

        // update cached values
        _keyboardHeight = height.toFloat()
        _keyboardOrientation = orientation

        // Log.d("Cubzh Keyboard height", "[onKeyboardHeightChanged] $_keyboardHeight | $statusBarHeight | $navigationBarHeight")

        if (_keyboardHeight < (navigationBarHeight.toFloat() + 1)) {
            _cubzhThreadHandler.post {
                keyboardDidClose(0.0f)
            }
            setSystemUIFlags()
        } else {
            _cubzhThreadHandler.post {
                keyboardDidOpen(0.0f, _keyboardHeight, 0.0f)
            }
        }
    }

    private fun didFinishLaunchingInternal(surface: Surface, width: Int, height: Int,
                                           topInset: Float, bottomInset: Float,
                                           leftInset: Float, rightInset: Float,
                                           pixelDensity: Float) {

        // TODO: if a new push token has been store for processing, send it to didFinishLaunching()
        // It is stored in GameApplication.shared().newPushTokenReceived

        _cubzhThreadHandler.post {
            didFinishLaunching(surface, width, height, topInset, bottomInset, leftInset, rightInset, pixelDensity)
        }

        if (!_tickScheduled) {
            // start Cubzh tick loop (w/ max framerate)
            class CubzhLoop : Runnable {
                private var lastTimestamp = System.currentTimeMillis()

                override fun run() {
                    val timestamp = System.currentTimeMillis()
                    val dt = timestamp - lastTimestamp
                    if (dt < DT_TARGET) {
                        _cubzhThreadHandler.postDelayed(this, DT_TARGET - dt)
                    } else {
                        if (!_paused) {
                            tickInternal(dt / 1000.0)
                        }
                        lastTimestamp = timestamp
                        _cubzhThreadHandler.post(this)
                    }
                }
            }
            _cubzhThreadHandler.post(CubzhLoop())
            _tickScheduled = true
        }
    }

    private fun tickInternal(dt: Double) { // called from Cubzh thread
        tick(dt)
        render()
    }

    private fun didResizeInternal(width: Int, height: Int, pixelsPerPoint: Float,
                                  topInset: Float, bottomInset: Float,
                                  leftInset: Float, rightInset: Float) {

        _cubzhThreadHandler.post {
            didResize(width, height, pixelsPerPoint, topInset, bottomInset, leftInset, rightInset)
        }
    }

    private fun willTerminateInternal() {
        _cubzhThreadHandler.post {
            willTerminate()
        }
    }

    private fun postTouchEventInternal(id: Int, x: Float, y: Float, dx: Float, dy: Float,
                                       state: Int, move: Boolean) {

        _cubzhThreadHandler.post {
            postTouchEvent(id, x, y, dx, dy, state, move)
        }
    }

    private fun setSystemUIFlags() {
        window.decorView.apply {
            systemUiVisibility =
                // content shouldn't resize when the system bars hide and show
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                // hide the nav/action bar and status bar
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                // sticky immersive mode:
                // - show semi-transparent system bars when swiped from top/bottom edge
                // - touch gesture is still passed to the app
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        }
    }

    @SuppressLint("InternalInsetResource")
    private fun getSystemBarInsets(view: View): Pair<Int, Int> {

        fun getStatusBarHeight(): Int {
            val resourceId = Resources.getSystem().getIdentifier("status_bar_height", "dimen", "android")
            return if (resourceId > 0) Resources.getSystem().getDimensionPixelSize(resourceId) else 0
        }

        fun getNavigationBarHeight(): Int {
            val resourceId = Resources.getSystem().getIdentifier("navigation_bar_height", "dimen", "android")
            return if (resourceId > 0) Resources.getSystem().getDimensionPixelSize(resourceId) else 0
        }

        return when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.R -> { // API 30 Android 11
                val insets = view.rootWindowInsets
                val statusBarHeight = insets?.getInsets(WindowInsetsCompat.Type.statusBars())?.top ?: 0
                val navigationBarHeight = insets?.getInsets(WindowInsetsCompat.Type.navigationBars())?.bottom ?: 0
                Pair(statusBarHeight, navigationBarHeight)
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q -> { // API 29 Android 10
                val insets = view.rootWindowInsets
                val statusBarHeight = insets?.systemWindowInsetTop ?: 0
                val navigationBarHeight = insets?.systemWindowInsetBottom ?: 0
                Pair(statusBarHeight, navigationBarHeight)
            }
            else -> { // earlier than API 29 Android 10
                val statusBarHeight = getStatusBarHeight()
                val navigationBarHeight = getNavigationBarHeight()
                Pair(statusBarHeight, navigationBarHeight)
            }
        }
    }

//    _inputMethodManager = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
//    private fun onWantTextInputChanged(value: Boolean) {
//        Log.v("Cubzh Java", "[VXLifecycle] onWantTextInputChanged $value")
//
//        // /!\ Calls order matters here, between focus & soft input toggle
//        if (value) {
//            toggleEditTextFocus(true)
//            resetEditText()
//            _sendTextInputEvent = true
//
//            _inputMethodManager.showSoftInput(_mainEditText, 0)
//        } else {
//            _inputMethodManager.hideSoftInputFromWindow(_glView.windowToken, 0)
//
//            _sendTextInputEvent = false
//            toggleEditTextFocus(false)
//
//            setSystemUIFlags()
//        }
//        _glView.setActionBarShown(value)
//    }
//
//    private fun resetEditText() {
//        _sendTextInputEvent = false
//        _mainEditText.setText(" ") // one space character to capture backspace
//        _mainEditText.setSelection(_mainEditText.length())
//        _sendTextInputEvent = true
//    }
//
//    private fun toggleEditTextFocus(value: Boolean) {
//        if (value) {
//            // Note: if EditText starts with size 0, it won't accept keyboard inputs
//            _mainEditText.layoutParams = RelativeLayout.LayoutParams(0, 0)
//            _mainEditText.visibility = View.VISIBLE
//            _mainEditText.requestFocus()
//        } else {
//            _mainEditText.clearFocus()
//            _mainEditText.visibility = View.INVISIBLE
//        }
//    }
//
//    private fun sendEditTextKey(key: Int) {
//        _cubzhThreadHandler.post {
//            postKeyboardInput(0, key, 0, INPUT_ACTION_DOWN)
//            postKeyboardInput(0, key, 0, INPUT_ACTION_UP)
//        }
//    }
//
//    private fun sendEditTextChar(charcode: Int, key: Int) {
//        _cubzhThreadHandler.post {
//            postKeyboardInput(charcode, key, 0, INPUT_ACTION_DOWN)
//            postKeyboardInput(charcode, key, 0, INPUT_ACTION_UP)
//        }
//    }
//
//    fun scheduleReminderLocalNotification(title: String, message: String, delaySec: Int) {
//        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
//            return
//        }
//
//        // Create a NotificationChannel
//        val channel = NotificationChannel(Notifications.notificationChannelReminders, "Reminder", NotificationManager.IMPORTANCE_DEFAULT)
//        val notificationManager = this.getSystemService(NotificationManager::class.java)
//        notificationManager.createNotificationChannel(channel)
//
//        // Calculate trigger time from delay
//        val calendar = Calendar.getInstance()
//        calendar.add(Calendar.SECOND, delaySec)
//
//        // Create an Intent for the BroadcastReceiver that will handle the notification
//        val intent = Intent(this, ReminderNotificationReceiver::class.java)
//        intent.putExtra(Notifications.intentExtraTitleKey, title)
//        intent.putExtra(Notifications.intentExtraMessageKey, message)
//        val pendingIntent = constructPendingIntent(intent, delaySec)
//
//        // Get the AlarmManager service
//        val alarmManager: AlarmManager = getSystemService(ALARM_SERVICE) as AlarmManager
//
//        // Schedule the alarm
//        alarmManager.set(AlarmManager.RTC_WAKEUP, calendar.timeInMillis, pendingIntent)
//    }
//
//    fun cancelReminderLocalNotificationsForDelaySec(delaySec: Int) {
//        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
//            return
//        }
//
//        val intent = Intent(this, ReminderNotificationReceiver::class.java)
//        val pendingIntent = constructPendingIntent(intent, delaySec)
//
//        val alarmManager = getSystemService(ALARM_SERVICE) as AlarmManager
//        alarmManager.cancel(pendingIntent)
//    }
//
//    private fun constructPendingIntent(intent: Intent, delaySec: Int): PendingIntent {
//        return PendingIntent.getBroadcast(
//            this,
//            delaySec, // requestCode
//            intent,
//            PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_UPDATE_CURRENT
//        )
//    }

}