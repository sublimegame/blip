package com.voxowl.blip

import android.util.Log
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage

class CubzhFirebaseInstanceIDService : FirebaseMessagingService() {

    /**
     * App received a push notification
     */
    override fun onMessageReceived(message: RemoteMessage) {
        val title: String = message.notification?.title ?: ""
        val body: String = message.notification?.body ?: ""
        val category: String = message.data["category"] ?: ""
        val badgeCount: Int = message.notification?.notificationCount ?: 0
        // val notificationID: String = message.data["notificationID"] ?: ""

        // Log.w("Cubzh Java", "[notification] onMessageReceived")
        // Log.w("Cubzh Java", "[notification] $title $body $category $badgeCount")

        // Notify the C++ layer
        onMessageReceivedCPP(title, body, category, badgeCount)
    }

    /**
     * Called if the FCM registration token is updated. This may occur if the security of
     * the previous token had been compromised. Note that this is called when the
     * FCM registration token is initially generated so this is where you would retrieve the token.
     */
    override fun onNewToken(token: String) {
        Log.d("Cubzh Java", "☀️[onNewToken] Refreshed token")

        // If you want to send messages to this application instance or
        // manage this apps subscriptions on the server side, send the
        // FCM registration token to your app server.

        // notify C++ layer (if it has been loaded in memory)
        if (GameApplication.getCPPLibLoaded()) {
            Log.d("Cubzh Java", "☀️[onNewToken] Token sent to C++")
            this.onNewPushTokenCPP(token);
        } else {
            Log.d("Cubzh Java", "☀️[onNewToken] Token stored for later processing")
            // store token in GameApplication, for it to be processed later
            GameApplication.shared().newPushTokenReceived = token;
        }
    }

    private external fun onMessageReceivedCPP(title: String, body: String, category: String, badge: Int)
    private external fun onNewPushTokenCPP(token: String)
}
