package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.core.app.NotificationCompat

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

    private val manager: NotificationManager = activity.getSystemService (Context.NOTIFICATION_SERVICE) as NotificationManager
    private val identifier = 1
    private var active: Int? = null

    private var previous: String = ""

    init {
        val channel = NotificationChannel (channelId, channelName, NotificationManager.IMPORTANCE_DEFAULT).apply {
            description = channelDescription
        }
        manager.createNotificationChannel (channel)
    }

    //

    private fun show (current: String) {
        if (current != previous) {
            val builder = NotificationCompat.Builder (activity, channelId)
                .setSmallIcon (android.R.drawable.stat_sys_warning)
                .setContentTitle (channelTitle)
                .setContentText (current)
                .setPriority (NotificationCompat.PRIORITY_DEFAULT)
                .setOngoing (true)
            manager.notify (identifier, builder.build ())
            active = identifier
            previous = current
        }
    }
    private fun reshow () {
        if (previous.isNotEmpty ()) {
            val current = previous
            previous = ""
            show (current)
        }
    }
    private fun clear () {
        active?.let {
            manager.cancel (it)
            active = null
        }
        previous = ""
    }

    //

    fun process (current: String) {
        if (current.isNotEmpty ()) {
            if (!permissions.requested) {
                previous = current
                permissions.requestPermissions(
                    onPermissionsAllowed = { reshow() }
                )
            } else if (!permissions.obtained) {
                previous = current
            } else if (permissions.allowed) {
                show (current)
            }
        } else
            clear ()
    }
}
