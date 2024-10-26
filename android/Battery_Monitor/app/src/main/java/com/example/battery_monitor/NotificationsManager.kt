package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context

@SuppressLint("MissingPermission")
class NotificationsManager(
    private val activity: Activity,
    private val config: NotificationsManagerConfig
) {
    private val permissions: PermissionsManager = PermissionsManagerFactory(activity).create(
        tag = "Notifications",
        permissionsRequired = arrayOf(android.Manifest.permission.POST_NOTIFICATIONS)
    )

    private val channelId = "AlarmChannel"

    private val manager: NotificationManager = activity.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    private val identifier = 1
    private var active: Int? = null
    private var previous: List<Pair<String, String>> = emptyList()

    init {
        val channel = NotificationChannel(channelId, config.name, NotificationManager.IMPORTANCE_DEFAULT).apply {
            description = config.description
        }
        manager.createNotificationChannel(channel)
    }

    //

    private fun show(current: List<Pair<String, String>>) {
        if (current != previous) {
            val title = if (current.isNotEmpty()) current.joinToString(", ") { it.first } else config.title
            val content = if (current.isNotEmpty()) current.joinToString("\n") { "${it.first}: ${it.second}" } else config.content
            val builder = Notification.Builder(activity, channelId)
                .setSmallIcon(R.drawable.ic_launcher)
                .setContentTitle(title)
                .setContentText(content)
                .setStyle(Notification.BigTextStyle().bigText(content))
                .setOngoing(true)
            manager.notify(identifier, builder.build())
            active = identifier
            previous = current
        }
    }

    private fun reshow() {
        if (previous.isNotEmpty()) {
            val current = previous
            previous = emptyList()
            show(current)
        }
    }

    private fun clear() {
        active?.let {
            manager.cancel(it)
            active = null
        }
        previous = emptyList()
    }

    //

    fun process(current: List<Pair<String, String>>) {
        if (current.isNotEmpty()) {
            when {
                !permissions.requested -> {
                    previous = current
                    permissions.requestPermissions(
                        onPermissionsAllowed = { reshow() }
                    )
                }
                !permissions.obtained -> previous = current
                permissions.allowed -> show(current)
            }
        } else {
            clear()
        }
    }
}
