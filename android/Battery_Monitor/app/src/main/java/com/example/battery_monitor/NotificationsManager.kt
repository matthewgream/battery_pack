package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context

@SuppressLint("MissingPermission", "InlinedApi")
class NotificationsManager (private val activity: Activity) {

    private val permissions: PermissionsManager = PermissionsManagerFactory (activity).create (
        tag = "Notifications",
        permissions = arrayOf (android.Manifest.permission.POST_NOTIFICATIONS)
    )

    private val channelId = "AlarmChannel"
    private val channelName = activity.getString (R.string.notification_channel_name)
    private val channelDescription = activity.getString (R.string.notification_channel_description)
    private val channelTitle = activity.getString (R.string.notification_channel_title)
    private val channelContent = activity.getString (R.string.notification_channel_content)

    private val manager: NotificationManager = activity.getSystemService (Context.NOTIFICATION_SERVICE) as NotificationManager
    private val identifier = 1
    private var active: Int? = null
    private var previous:  List <Pair <String, String>> = emptyList()

    init {
        val channel = NotificationChannel (channelId, channelName, NotificationManager.IMPORTANCE_DEFAULT).apply {
            description = channelDescription
        }
        manager.createNotificationChannel (channel)
    }

    //

    private fun show (current: List<Pair<String, String>>) {
        if (current != previous) {
            val title = if (current.isNotEmpty ()) current.joinToString (", ") { it.first } else channelTitle
            val content = if (current.isNotEmpty ()) current.joinToString ("\n") { "${it.first}: ${it.second}" } else channelContent
            val builder = Notification.Builder (activity, channelId)
                .setSmallIcon (R.drawable.ic_temp_fan)
                .setContentTitle (title)
                .setContentText (content)
                .setStyle (Notification.BigTextStyle ().bigText (content))
                .setOngoing (true)
            manager.notify (identifier, builder.build ())
            active = identifier
            previous = current
        }
    }
    private fun reshow () {
        if (previous.isNotEmpty ()) {
            val current = previous
            previous =  emptyList ()
            show (current)
        }
    }
    private fun clear () {
        active?.let {
            manager.cancel (it)
            active = null
        }
        previous = emptyList ()
    }

    //

    fun process (current: List<Pair<String, String>>) {
        if (current.isNotEmpty ()) {
            when {
                !permissions.requested -> {
                    previous = current
                    permissions.requestPermissions (
                        onPermissionsAllowed = { reshow () }
                    )
                }
                !permissions.obtained -> previous = current
                permissions.allowed ->  show (current)
            }
        } else {
            clear ()
        }
    }
}
