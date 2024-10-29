package com.example.battery_monitor.utility

import android.annotation.SuppressLint
import android.app.Activity
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.annotation.DrawableRes

@SuppressLint("MissingPermission")
class NotificationsManager(
    private val activity: Activity,
    private val config: Config,
    @DrawableRes val iconResourceId: Int
) {

    open class Config(
        val name: String,
        val description: String,
    )

    private val permissions: PermissionsManager = PermissionsManagerFactory(activity).create("Notifications",
        arrayOf(
            android.Manifest.permission.POST_NOTIFICATIONS
        )
    )

    private val channel = "AlarmChannel"
    private val manager: NotificationManager = activity.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    init {
        manager.createNotificationChannel(
            NotificationChannel(channel, config.name, NotificationManager.IMPORTANCE_DEFAULT).apply { description = config.description }
        )
    }

    //

    private val identifier = 1
    private var active = Activable()
    private var previous: List<Pair<String, String>> = emptyList()
    private fun show(current: List<Pair<String, String>>) {
        if (current != previous) {
            val title = current.joinToString(", ") { it.first }
            val text = current.joinToString("\n") { "${it.first}: ${it.second}" }
            manager.notify(identifier,
                Notification.Builder(activity, channel)
                    .setSmallIcon(iconResourceId)
                    .setContentTitle(title)
                    .setContentText(text)
                    .setStyle(Notification.BigTextStyle().bigText(text))
                    .setOngoing(true)
                    .build()
            )
            active.isActive = true
            previous = current
        }
    }
    private fun showAgain() {
        if (previous.isNotEmpty()) {
            val current = previous
            previous = emptyList()
            show(current)
        }
    }
    private fun clear() {
        if (active.toInactive())
            manager.cancel(identifier)
        previous = emptyList()
    }

    //

    fun process(current: List<Pair<String, String>>) {
        if (current.isNotEmpty()) {
            when {
                !permissions.requested -> {
                    previous = current
                    permissions.requestPermissions(
                        onPermissionsAllowed = { showAgain() }
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
