package com.example.battery_monitor

object DataManagerAlarm {
    data class Alarm(val code: String, val name: String, val description: String)

    private val alarmMap = mapOf(
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

    fun translateAlarms(alarmCodes: String): List<Pair<String, String>> {
        return alarmCodes.split(",").mapNotNull { alarmMap[it.trim()] }.map { Pair(it.name, it.description) }
    }
}