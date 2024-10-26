package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.widget.TextView
import org.json.JSONObject
import kotlin.math.floor

class DataProcessorStatus(private val activity: Activity) {

    private val addrTextView: TextView = activity.findViewById(R.id.addrTextView)
    private val timeTextView: TextView = activity.findViewById(R.id.timeTextView)
    private val envTempTextView: TextView = activity.findViewById(R.id.envTempTextView)
    private val batTempTextView: TextView = activity.findViewById(R.id.batTempTextView)
    private val batTempValuesView: DataViewBatteryTemperature =
        activity.findViewById(R.id.batTempValuesView)
    private val fanSpeedTextView: TextView = activity.findViewById(R.id.fanSpeedTextView)
    private val alarmsTextView: TextView = activity.findViewById(R.id.alarmsTextView)

    @SuppressLint("SetTextI18n")
    fun render(json: JSONObject) {
        activity.runOnUiThread {
            val addr = json.getString("addr")
            addrTextView.text = if (addr == SECRET_DEVICE_ADDR) {
                "$SECRET_DEVICE_NAME ($addr)"
            } else {
                addr
            }
            timeTextView.text = json.getString("time")

            val env = json.getJSONObject("tmp").getDouble("env")
            envTempTextView.text = "%.1f°C (ext)".format(env)
            val bat = json.getJSONObject("tmp").getJSONObject("bat")
            batTempTextView.text = "%.1f°C (avg), %.1f°C (min), %.1f°C (max)".format(
                bat.getDouble("avg"), bat.getDouble("min"), bat.getDouble("max")
            )
            val vat = bat.getJSONArray("val")
            batTempValuesView.setTemperatureValues((0 until vat.length()).map {
                vat.getDouble(it).toFloat()
            })

            val fan = floor(100 * json.getInt("fan").toDouble() / 255).toInt()
            fanSpeedTextView.text = "$fan % (fan multiMap)"

            val alarmPairs = DataManagerAlarm.translateAlarms(json.getString("alm"))
            alarmsTextView.text = if (alarmPairs.isNotEmpty()) {
                alarmPairs.joinToString(", ") { it.first }
            } else {
                "No alarms"
            }
            alarmsTextView.setTextColor(activity.getColor(if (alarmPairs.isNotEmpty()) R.color.alarm_active_color else R.color.alarm_inactive_color))
        }
    }
}
