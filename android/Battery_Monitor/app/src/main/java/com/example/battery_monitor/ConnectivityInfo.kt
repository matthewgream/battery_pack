package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Build
import org.json.JSONObject
import java.time.Instant
import java.time.format.DateTimeFormatter

class ConnectivityInfo (activity: Activity) {

    private val prefs: SharedPreferences = activity.getSharedPreferences ("ConnectionInfo", Context.MODE_PRIVATE)

    private val appName = "batterymonitor"
    private val appVersion = try {
        activity.packageManager.getPackageInfo(activity.packageName, PackageManager.PackageInfoFlags.of(0)).versionName
    } catch (e: PackageManager.NameNotFoundException) {
        "?.?.?"
    }
    private val appPlatform = "android${Build.VERSION.SDK_INT}"
    private val appDevice = "${Build.MANUFACTURER} ${Build.MODEL}"

    var deviceAddress: String = ""
        private set

    init {
        deviceAddress = prefs.getString ("device_address", "") ?: ""
    }

    fun updateDeviceAddress (newAddress: String) {
        deviceAddress = newAddress
        prefs.edit ().putString ("device_address", newAddress).apply ()
    }

    fun toJsonString (): String {
        return JSONObject ().apply {
            put ("type", "info")
            put ("time", DateTimeFormatter.ISO_INSTANT.format(Instant.now()))
            put ("info", "$appName-custom-$appPlatform-v$appVersion ($appDevice)")
        }.toString ()
    }
}