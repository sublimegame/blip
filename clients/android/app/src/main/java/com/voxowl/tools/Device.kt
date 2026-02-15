package com.voxowl.tools

import android.app.ActivityManager
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Build
import android.os.CombinedVibration
import android.os.Handler
import android.os.VibrationAttributes
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.provider.Settings
import android.util.Log
import android.widget.Toast
import com.voxowl.blip.BuildConfig
import com.voxowl.blip.MainActivity
import java.util.Locale
import java.util.concurrent.Callable
import java.util.concurrent.ExecutionException
import java.util.concurrent.FutureTask

class Device {

    companion object {

        @JvmStatic
        fun getBuildVersionSdkInt(): Int {
            return Build.VERSION.SDK_INT
        }

        @JvmStatic
        fun getHardwareBrand(): String {
            return Build.BRAND
        }

        @JvmStatic
        fun getHardwareModel(): String {
            return Build.MODEL
        }

        @JvmStatic
        fun getHardwareProduct(): String {
            return Build.PRODUCT
        }

        @JvmStatic
        fun getHardwareMemory(): Long {
            val activity = MainActivity.shared ?: return 0 // returns if null
            val actManager = activity.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val memInfo = ActivityManager.MemoryInfo()
            actManager.getMemoryInfo(memInfo)
            return memInfo.totalMem
        }

        @JvmStatic
        fun getAppVersionName(): String {
            val activity = MainActivity.shared ?: return "" // returns if null
            try {
                val info: PackageInfo = activity.packageManager.getPackageInfo(activity.packageName, 0)
                return info.versionName ?: ""
            } catch (e: PackageManager.NameNotFoundException) {
                return ""
            }
        }

        @JvmStatic
        fun getAppVersionCode(): Int {
            return BuildConfig.VERSION_CODE
        }

        @JvmStatic
        fun getClipboardText(): String {
            // ClipboardManager can only be accessed in the UI-thread

            // So we access the clipboard asynchronously in the UI-thread
            // and get the content back to the calling thread.
            // This prevents a crash on Android 7.x and 8.x (at least).

            val activity = MainActivity.shared ?: return "" // returns if activity is null

            val futureResult = FutureTask<String>(Callable {
                val clipboard = activity.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = clipboard.primaryClip ?: return@Callable ""
                // look for text in the clipboard items
                var i = 0
                while (i < clip.itemCount) {
                    val str = clip.getItemAt(i).text
                    if (str != null) {
                        return@Callable str.toString()
                    }
                    i += 1
                }
                ""
            })

            activity.runOnUiThread(futureResult) // this blocks until the result is calculated!

            var result = ""

            try {
                result = futureResult.get()
            } catch (exception: InterruptedException) {
                val cause = exception.cause
                Log.e("Cubzh Java", "getClipboardText has thrown an exception", cause)
            } catch (exception: ExecutionException) {
                val cause = exception.cause
                Log.e("Cubzh Java", "getClipboardText has thrown an exception", cause)
            }

            return result
        }

        @JvmStatic
        fun setClipboardText(text: String) {
            val activity = MainActivity.shared ?: return // returns if activity is null
            activity.runOnUiThread {
                val clipboard = activity.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                clipboard.setPrimaryClip(ClipData.newPlainText("copied text", text))
            }
        }

        @JvmStatic
        fun hapticImpactLight() {
            var effect = 0
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // API >= 29
                effect = VibrationEffect.EFFECT_TICK
            }
            hapticImpact(effect, 15, 30)
        }

        @JvmStatic
        fun hapticImpactMedium() {
            var effect = 0
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // API >= 29
                effect = VibrationEffect.EFFECT_CLICK
            }
            hapticImpact(effect, 15, 60)
        }


        @JvmStatic
        fun hapticImpactHeavy() {
            var effect = 0
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // API >= 29
                effect = VibrationEffect.EFFECT_HEAVY_CLICK
            }
            hapticImpact(effect, 15, 120)
        }

        @JvmStatic
        fun openApplicationSettings() {
            val activity = MainActivity.shared ?: return // returns if activity is null
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val settingsIntent: Intent = Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(Settings.EXTRA_APP_PACKAGE, activity.packageName)
                //.putExtra(Settings.EXTRA_CHANNEL_ID, MY_CHANNEL_ID);
                activity.startActivity(settingsIntent)
            } else {
                // Get a handler that can be used to post to the main thread
                val mainHandler = Handler(activity.mainLooper)
                val myRunnable = Runnable { // This is your code
                    Toast.makeText(
                        activity,
                        "unsupported action",
                        Toast.LENGTH_LONG
                    ).show()
                }
                mainHandler.post(myRunnable)
            }
        }

        @JvmStatic
        fun getPerformanceTier(): Int {
            // Note: available RAM is typically lower than advertised RAM,
            // eg. Google Pixel 3 advertises 4GB, but here we'll get ~3.4GB
            val memGB = getHardwareMemory() / 1073741824.0;
            val tier = if (memGB > 6) { 3 } // 6+ RAM
                else if (memGB > 4) { 2 }   // 5-6 RAM
                else if (memGB > 2) { 1 }   // 3-4 RAM
                else { 0 }                  // 2- RAM
            return tier;
        }

        @JvmStatic
        fun getPreferredLanguages(): String {
            return Locale.getDefault().toLanguageTag() // "en-US", "fr-FR", ...
        }

        // Haptic feedback documentation: https://developer.android.com/develop/ui/views/haptics
        private fun hapticImpact(effectID: Int, duration: Long, amplitude: Int) {
            val activity = MainActivity.shared ?: return // returns if activity is null

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // API >= 31
                val vibratorManager = activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                val ve = VibrationEffect.createPredefined(effectID)
                val combinedVibration = CombinedVibration.createParallel(ve)
                val attrs = VibrationAttributes.Builder().setUsage(VibrationAttributes.USAGE_HARDWARE_FEEDBACK).build()
                vibratorManager.vibrate(combinedVibration, attrs)
            } else {
                // API < 31
                @Suppress("DEPRECATION")
                val vibrator = activity.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                if (!vibrator.hasVibrator()) {
                    return
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) { // API 29
                    val ve = VibrationEffect.createPredefined(effectID)
                    vibrator.vibrate(ve)
                } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) { // API 26
                    val ve = VibrationEffect.createOneShot(duration, amplitude)
                    vibrator.vibrate(ve)
                }
            }
        }

        @JvmStatic
        fun setLandscape() {
            MainActivity.shared?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }

        @JvmStatic
        fun setPortrait() {
            MainActivity.shared?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        }

        @JvmStatic
        fun setDefaultOrientation() {
            MainActivity.shared?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
    }
}
