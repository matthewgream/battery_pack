package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.widget.TextView
import org.json.JSONObject

class DataOperationalHandler (private val activity: Activity) {

    private val timeTextView: TextView = activity.findViewById(R.id.timeTextView)
    private val envTempTextView: TextView = activity.findViewById(R.id.envTempTextView)
    private val batteryLabelTextView: TextView = activity.findViewById(R.id.batteryLabelTextView)
    private val batteryTempMinTextView: TextView = activity.findViewById(R.id.batteryTempMinTextView)
    private val batteryTempAvgTextView: TextView = activity.findViewById(R.id.batteryTempAvgTextView)
    private val batteryTempMaxTextView: TextView = activity.findViewById(R.id.batteryTempMaxTextView)
    private val batteryTempValuesView: BatteryTemperatureView = activity.findViewById(R.id.batteryTempValuesView)
    private val fanSpeedTextView: TextView = activity.findViewById(R.id.fanSpeedTextView)
    private val alarmsTextView: TextView = activity.findViewById(R.id.alarmsTextView)

    @SuppressLint("SetTextI18n")
    fun render(json: JSONObject) {
        activity.runOnUiThread {
            timeTextView.text = json.getString("time")
            val env = json.getJSONObject("tmp").getDouble("env")
            envTempTextView.text = "$env°C"

            batteryLabelTextView.text = "Battery:"
            val bat = json.getJSONObject("tmp").getJSONObject("bat")
            batteryTempAvgTextView.text = "Avg: ${bat.getDouble("avg")}°C"
            batteryTempMinTextView.text = "Min: ${bat.getDouble("min")}°C"
            batteryTempMaxTextView.text = "Max: ${bat.getDouble("max")}°C"
            val vat = bat.getJSONArray("val")
            batteryTempValuesView.setTemperatureValues((0 until vat.length()).map { vat.getDouble(it).toFloat() })

            val fan = json.getInt("fan")
            fanSpeedTextView.text = "Fan: $fan %"

            val alm = json.getString("alm")
            alarmsTextView.text = "Alarms: $alm"
            alarmsTextView.setTextColor(activity.getColor(if (alm.isNotEmpty()) R.color.alarm_active_color else R.color.alarm_inactive_color))
        }
    }
}
