package com.voxowl.blip

import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import androidx.core.app.NotificationCompat
import com.voxowl.tools.Notifications

class ReminderNotificationReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {

        // Retrieve extra info from intent
        val title: String = intent.getStringExtra(Notifications.intentExtraTitleKey) ?: ""
        val message: String = intent.getStringExtra(Notifications.intentExtraMessageKey) ?: ""

        // Build and show the notification
        showNotification(context, title, message)
    }

    private fun showNotification(context: Context, title: String, message: String) {
        // Create an Intent for launching the app when the notification is clicked
        val notificationIntent = Intent(context, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            context,
            0,
            notificationIntent,
            PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE)

        // Create a large icon from the resource
        val largeIconBitmap = BitmapFactory.decodeResource(context.resources, R.mipmap.ic_launcher)

        // Build the notification
        val builder: NotificationCompat.Builder = NotificationCompat.Builder(context, Notifications.notificationChannelReminders)
            .setSmallIcon(R.drawable.ic_blip)
            .setLargeIcon(largeIconBitmap)
            .setContentTitle(title)
            .setContentText(message)
            .setContentIntent(pendingIntent)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setAutoCancel(true)

        // Get the NotificationManager
        val notificationManager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        // Notify
        notificationManager.notify(0, builder.build())
    }
}
