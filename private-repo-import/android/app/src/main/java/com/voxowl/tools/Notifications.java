package com.voxowl.tools;

import android.Manifest;
import android.app.NotificationManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.core.content.ContextCompat;
import com.google.firebase.messaging.FirebaseMessaging;
import com.voxowl.blip.MainActivity;

public class Notifications {

    /**
     * Used for local reminder notifications
     */
    static public final String notificationChannelReminders = "czh_reminders_channel";
    /**
     * Used for local reminder notifications
     */
    static public final String intentExtraTitleKey = "title";
    /**
     * Used for local reminder notifications
     */
    static public final String intentExtraMessageKey = "message";

    static private final int NotificationAuthorizationResponse_Authorized = 1;
    static private final int NotificationAuthorizationResponse_Denied = 2;

    /**
     *
     */
    static public void init(Context ctx, MainActivity activity) {
        Notifications.applicationContext = ctx;
        Notifications.activity = activity;
        Notifications._requestPermissionLauncher = activity.registerForActivityResult(new ActivityResultContracts.RequestPermission(), isGranted -> {
            if (isGranted) {
                // Permission is granted. Continue the action or workflow in your app.
                // FCM SDK (and your app) can post notifications.
                Log.i("Cubzh Java", "📣Notifications permission granted by user");

                // This will create the file in storage to indicate the permission has been asked
                setRemotePushAuthorization();

                // call C++ callback for notification permission
                didReplyToNotificationPermissionPopup(NotificationAuthorizationResponse_Authorized);

                // request Firebase Push Token (Cloud Messaging)
                Notifications.requestFirebaseToken();
            } else {
                // Explain to the user that the feature is unavailable because the
                // feature requires a permission that the user has denied. At the
                // same time, respect the user's decision. Don't link to system
                // settings in an effort to convince the user to change their
                // decision.
                Log.i("Cubzh Java", "📣Notifications permission refused by user");

                // This will create the file in storage to indicate the permission has been asked
                setRemotePushAuthorization();

                // call C++ callback for notification permission
                didReplyToNotificationPermissionPopup(NotificationAuthorizationResponse_Denied);
            }
        });
        Notifications._permissionName = _getPostNotificationsPermissionName();
    }

    /**
     * Returns true if Post Notifications permission has been granted, false otherwise.
     */
    static public boolean permissionGranted() {
        final int granted = ContextCompat.checkSelfPermission(applicationContext, Notifications._permissionName);
        final boolean isGranted = granted == PackageManager.PERMISSION_GRANTED;
        Log.i("Cubzh Java", "📣Notifications permission granted ? "+isGranted);
        return isGranted;
    }
    
//    /**
//     * @return whether an info popup, explaining the need for notifications permission, should be displayed
//     */
//    static public boolean shouldShowInfoPopup() {
//        boolean result = false;
//        if (!permissionGranted()) {
//            result = activity.shouldShowRequestPermissionRationale(Notifications._permissionName);
//        }
//        Log.i("Cubzh Java", "📣 shouldShowInfoPopup: " + result);
//        return result;
//    }

    /**
     *
     */
    static public void requestPermission() {
        if (permissionGranted()) {
            Log.i("Cubzh Java", "📣 Post Notifications permission already granted");

            // This will create the file in storage to indicate the permission has been asked
            setRemotePushAuthorization();

            // call C++ callback for notification permission
            didReplyToNotificationPermissionPopup(NotificationAuthorizationResponse_Authorized);

            // request Firebase Push Token (Cloud Messaging)
            Notifications.requestFirebaseToken();

        } else {
            Log.i("Cubzh Java", "📣 request Post Notifications permission!");
            // Directly ask for the permission
            _requestPermissionLauncher.launch(Notifications._permissionName);
        }
    }

    /**
     *
     */
    static public void requestFirebaseToken() {
        FirebaseMessaging.getInstance().getToken().addOnCompleteListener(task -> {
            if (!task.isSuccessful()) {
                Log.w("Cubzh", "Fetching FCM registration token failed", task.getException());
                return;
            }
            activity.getCubzhThreadHandler().post(new Runnable() {
                @Override
                public void run() {
                    // Get new FCM registration token
                    String token = task.getResult();
                    Notifications.didRegisterForRemoteNotifications(token);
                }
            });
        });
    }

    static public void setBadgeCount(int count) {
        // Log.w("Cubzh Java", "[notifications] setBadgeCount");
        if (count == 0) {
            // Clear all notifications in Android panel (and thus remove app icon badge)
            NotificationManager m = (NotificationManager) activity.getSystemService(Context.NOTIFICATION_SERVICE);
            m.cancelAll();
        }
    }

    /**
     * Schedule all reminder local notifications
     */
    static public boolean scheduleAllLocalNotificationReminders() {
        // activity.scheduleReminderLocalNotification("A Week's Worth of Wonders!", "✨ Cubzh is a constantly evolving universe, come see what's new!", 7*86400);
        // activity.scheduleReminderLocalNotification("Two Weeks' Worth of Wonders!", "✨ Cubzh is a constantly evolving universe, come see what's new!", 14*86400);
        // activity.scheduleReminderLocalNotification("A Month's Worth of Wonders!", "✨ Cubzh is a constantly evolving universe, come see what's new!", 30*86400);
        return true;
    }

    /**
     * Cancel all reminder local notifications
     */
    static public boolean cancelAllLocalNotificationReminders() {
        // activity.cancelReminderLocalNotificationsForDelaySec(7*86400);
        // activity.cancelReminderLocalNotificationsForDelaySec(14*86400);
        // activity.cancelReminderLocalNotificationsForDelaySec(30*86400);
        return true;
    }

    // --------------------------------------------------
    //
    // Local functions
    //
    // --------------------------------------------------

    static private String _getPostNotificationsPermissionName() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return Manifest.permission.POST_NOTIFICATIONS;
        }
        return "android.permission.POST_NOTIFICATIONS";
    }

    // equivalent to vx::notification::setRemotePushAuthorization
    static private native void setRemotePushAuthorization();

    static private native void didReplyToNotificationPermissionPopup(int responseEnumValue);

    static private native void didRegisterForRemoteNotifications(final String token);

    // --------------------------------------------------
    //
    // Local fields
    //
    // --------------------------------------------------

    static private Context applicationContext = null;
    static private MainActivity activity = null;
    static private ActivityResultLauncher<String> _requestPermissionLauncher = null;
    static private String _permissionName = "";
}
