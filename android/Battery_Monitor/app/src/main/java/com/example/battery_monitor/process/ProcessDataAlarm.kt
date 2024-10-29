package com.example.battery_monitor.process

import android.app.Activity
import android.widget.TextView
import com.example.battery_monitor.R
import com.example.battery_monitor.utility.NotificationsManager
import org.json.JSONObject

class ProcessDataAlarm(
    private val activity: Activity,
    private val notificationsManager: NotificationsManager
) {

    private val view: TextView = activity.findViewById(R.id.processAlarmTextView)

    data class Alarm(val code: String, val name: String, val description: String)
    private val alarms = mapOf(
        "TIME_SYNC" to Alarm("TIME_SYNC", "Time sync", "Device time failed network sync"),
        "TIME_DRIFT" to Alarm("TIME_DRIFT", "Time drift", "Device time drifting significantly"),
        "TEMP_FAIL" to Alarm("TEMP_FAIL", "Temp fail", "Temperature sensor failed"),
        "TEMP_MIN" to Alarm("TEMP_MIN", "Temp low", "Temperature below minimum level"),
        "TEMP_WARN" to Alarm("TEMP_WARN", "Temp warn", "Temperature reaching maximum level"),
        "TEMP_MAX" to Alarm("TEMP_MAX", "Temp high", "Temperature exceeded maximum level"),
        "STORE_FAIL" to Alarm("STORE_FAIL", "Store fail", "Storage system has failed"),
        "STORE_SIZE" to Alarm("STORE_SIZE", "Store full", "Storage system is full"),
        "PUBLISH_FAIL" to Alarm("PUBLISH_FAIL", "Publish fail", "Publish to network (MQTT) failed"),
        "PUBLISH_SIZE" to Alarm("PUBLISH_SIZE", "Publish size", "Publish to network (MQTT) too large"),
        "DELIVER_FAIL" to Alarm("DELIVER_FAIL", "Deliver fail", "Deliver to device (BLE) failed"),
        "DELIVER_SIZE" to Alarm("DELIVER_SIZE", "Deliver size", "Deliver to device (BLE) too large"),
        "UPDATE_VERS" to Alarm("UPDATE_VERS", "Device update", "Device update available"),
        "UPDATE_LONG" to Alarm("UPDATE_LONG", "Device check", "Device update check needed"),
        "SYSTEM_MEMLOW" to Alarm("SYSTEM_MEMLOW", "Device memory", "Device memory low"),
        "SYSTEM_BADRESET" to Alarm("SYSTEM_BADRESET", "Device fault", "Device reset unexpectedly")
    )
    private fun translate(alarmCodes: String): List<Pair<String, String>> {
        return alarmCodes.split(",").mapNotNull { alarms[it.trim()] }.map { Pair(it.name, it.description) }
    }

    fun render (json: JSONObject) {
        if (json.has("alm"))
            notificationsManager.process(translate(json.getString("alm")))
        val alarmPairs = translate(json.getString("alm"))
        view.text = if (alarmPairs.isNotEmpty()) { alarmPairs.joinToString(", ") { it.first } } else { "No alarms" }
        view.setTextColor(activity.getColor(if (alarmPairs.isNotEmpty()) R.color.process_alarm_active_color else R.color.process_alarm_inactive_color))
    }
}