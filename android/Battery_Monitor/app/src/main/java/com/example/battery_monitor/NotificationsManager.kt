package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.pm.PackageManager
import android.util.Log
import androidx.core.app.NotificationCompat

@SuppressLint("MissingPermission")
class NotificationsManager (private val activity: Activity) {

    val PERMISSIONS_CODE = 2
    private var permissionsRequested = false
    private var permissionsObtained = false
    private var permissionsAllowed = false
    @SuppressLint("InlinedApi")
    private val permissionsList = arrayOf (android.Manifest.permission.POST_NOTIFICATIONS)

    private var previous: String = ""
    private var manager: NotificationManager
    private val identifier = 1
    private val channelId = "AlarmChannel"
    private var active: Int? = null

    init {
        val channelName = activity.getString (R.string.channel_name)
        val channelDescription = activity.getString (R.string.channel_description)
        val channel = NotificationChannel (channelId, channelName, NotificationManager.IMPORTANCE_DEFAULT).apply {
            description = channelDescription
        }
        manager = activity.getSystemService (Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.createNotificationChannel (channel)
    }

    //

    private fun show (current: String) {
        if (current != previous) {
            val builder = NotificationCompat.Builder (activity, channelId)
                .setSmallIcon (android.R.drawable.stat_sys_warning)
                .setContentTitle ("Battery Monitor Alarm")
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
            if (!permissionsRequested) {
                previous = current
                permissionsRequest ()
            } else if (!permissionsObtained) {
                previous = current
            } else if (permissionsAllowed) {
                show (current)
            }
        } else
            clear ()
    }

    //

    private fun onPermissionsAllowed () {
        reshow ()
    }
    private fun permissionsRequest () {
        permissionsRequested = true
        if (!permissionsList.all { activity.checkSelfPermission (it) == PackageManager.PERMISSION_GRANTED }) {
            Log.d ("Notifications", "Permissions request")
            activity.requestPermissions (permissionsList, PERMISSIONS_CODE)
        } else {
            Log.d ("Notifications", "Permissions granted already")
            permissionsObtained = true
            permissionsAllowed = true
            onPermissionsAllowed ()
        }
    }
    fun permissionsHandler (grantResults: IntArray) {
        permissionsObtained = true
        if (grantResults.isNotEmpty () && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            Log.d("Notifications", "Permissions granted")
            permissionsAllowed = true
            onPermissionsAllowed ()
        } else {
            Log.d("Notifications", "Permissions denied")
        }
    }
}
