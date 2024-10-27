package com.example.battery_monitor

import android.app.Activity
import android.util.Log
import org.json.JSONObject

class DataProcessor(
    activity: Activity,
    private val notificationsManager: NotificationsManager
) {
    private val tag = "DataProcessor"
    private val dataProcessorDiagnostic: DataProcessorDiagnostic = DataProcessorDiagnostic(activity)
    private val dataProcessorStatus: DataProcessorStatus = DataProcessorStatus(activity)

    private val timestampLastByType = mutableMapOf<String, String>()
    private fun timestampEqualOrNewer(type: String, time: String): Boolean {
        val timeLast = timestampLastByType [type]
        return if (timeLast == null) { true } else { time >= timeLast }
    }

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
                if (!json.getJSONObject("tmp").optDouble("env", Double.NaN).isNaN ())
                    dataProcessorStatus.render(json)
                if (json.has("alm"))
                    notificationsManager.process(DataManagerAlarm.translateAlarms(json.getString("alm")))
            }
            "diag" -> {
                dataProcessorDiagnostic.render(json)
            }
            else -> Log.w(tag, "JSON type unknown: type=$type")
        }
    }
}