package com.example.battery_monitor.process

import android.app.Activity
import android.util.Log
import com.example.battery_monitor.utility.NotificationsManager
import org.json.JSONObject

class ProcessManager(
    private val tag: String,
    activity: Activity,
    notificationsManager: NotificationsManager,
    addressMapper: (String) -> String = { addr -> addr }
) {
    private val processDataDiagnostic: ProcessDataDiagnostic = ProcessDataDiagnostic(activity)
    private val processDataStatus: ProcessDataStatus = ProcessDataStatus(activity, addressMapper)
    private val processDataAlarm: ProcessDataAlarm = ProcessDataAlarm(activity, notificationsManager)

    private val timestampLastByType = mutableMapOf<String, String>()
    private fun timestampEqualOrNewer(type: String, time: String) =
        timestampLastByType[type]?.let { time >= it } ?: true

    fun processDataReceived(json: JSONObject) {
        val type = json.getString("type")
        val time = json.getString("time")
        if (!timestampEqualOrNewer(type, time)) {
            Log.w(tag, "Rejected out of order data: type=$type, time=$time, timeLast=${timestampLastByType[type]}")
            return
        }
        timestampLastByType[type] = time
        when (type) {
            "data" -> {
                processDataStatus.render(json)
                processDataAlarm.render(json)
            }
            "diag" -> {
                processDataDiagnostic.render(json)
            }
            else -> Log.w(tag, "JSON type unknown: type=$type")
        }
    }
}