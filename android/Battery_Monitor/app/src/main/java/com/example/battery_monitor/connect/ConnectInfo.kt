package com.example.battery_monitor.connect

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Build
import org.json.JSONObject
import java.time.Instant
import java.time.format.DateTimeFormatter

class ConnectInfo(
    activity: Activity,
    val name: String
) {
    private val prefs: SharedPreferences = activity.getSharedPreferences("ConnectionInfo", Context.MODE_PRIVATE)

    private val version = try {
        activity.packageManager.getPackageInfo(activity.packageName, PackageManager.PackageInfoFlags.of(0)).versionName
    } catch (e: PackageManager.NameNotFoundException) {
        "?.?.?"
    }
    private val platform = "android${Build.VERSION.SDK_INT}"
    private val device = "${Build.MANUFACTURER} ${Build.MODEL}"
    var identity: String = "$name-custom-$platform-v$version ($device)"
        private set

    var deviceAddress: String = ""
        private set

    init {
        deviceAddress = prefs.getString("device_address", "") ?: ""
    }

    fun updateDeviceAddress(newAddress: String) {
        deviceAddress = newAddress
        prefs.edit().putString("device_address", newAddress).apply()
    }

    fun toJsonString(): String {
        return JSONObject().apply {
            put("type", "info")
            put("time", DateTimeFormatter.ISO_INSTANT.format(Instant.now()))
            put("info", identity)
            put("peer", deviceAddress)
        }.toString()
    }
}