package com.example.battery_monitor

object DataManagerAlarm {
    data class Alarm (val code: String, val name: String, val description: String)
    private val alarmMap = mapOf (
        "TIME_SYNC" to Alarm("TIME_SYNC", "Time sync", "Device time failed network sync"),
        "TIME_DRIFT" to Alarm("TIME_DRIFT", "Time drift", "Device time drifting significantly"),
        "TEMP_FAIL" to Alarm("TEMP_FAIL", "Temp fail", "Temperature sensor failed"),
        "TEMP_MIN" to Alarm("TEMP_MIN", "Temp low", "Temperature below minimum level"),
        "TEMP_WARN" to Alarm("TEMP_WARN", "Temp warn", "Temperature reaching maximum level"),
        "TEMP_MAX" to Alarm("TEMP_MAX", "Temp high", "Temperature exceeded maximum level"),
        "STORE_FAIL" to Alarm("STORE_FAIL", "Store fail", "Storage system has failed"),
        "STORE_SIZE" to Alarm("STORE_SIZE", "Store full", "Storage system is full"),
        "PUBLISH_FAIL" to Alarm("PUBLISH_FAIL", "Publish fail", "Publish to network (MQTT) failed"),
        "DELIVER_SIZE" to Alarm("DELIVER_SIZE", "Deliver size", "Deliver size (BLE) was exceeded"),
        "UPDATE_VERS" to Alarm("UPDATE_VERS", "Device update", "Device update available"),
        "SYSTEM_MEMLOW" to Alarm("SYSTEM_MEMLOW", "Device memory", "Device memory low"),
        "SYSTEM_BADRESET" to Alarm("SYSTEM_BADRESET", "Device fault", "Device reset unexpectedly")
    )
    fun translateAlarms (alarmCodes: String): List<Pair<String, String>> {
        return alarmCodes.split(",").mapNotNull { alarmMap[it.trim()]}.map { Pair(it.name, it.description) }
    }
}