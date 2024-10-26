package com.example.battery_monitor

import android.app.Activity
import android.util.Log
import org.json.JSONObject

class DataProcessor(
    activity: Activity,
    private val notificationsManager: NotificationsManager
) {
    private val dataProcessorDiagnostic: DataProcessorDiagnostic = DataProcessorDiagnostic(activity)
    private val dataProcessorStatus: DataProcessorStatus = DataProcessorStatus(activity)

    fun processDataReceived(json: JSONObject) {
        when (val type = json.getString("type")) {
            "data" -> {
                dataProcessorStatus.render(json)
                notificationsManager.process(DataManagerAlarm.translateAlarms(json.getString("alm")))
            }
            "diag" -> dataProcessorDiagnostic.render(json)
            else -> Log.w("DataProcessor", "JSON type unknown: $type")
        }
    }
}