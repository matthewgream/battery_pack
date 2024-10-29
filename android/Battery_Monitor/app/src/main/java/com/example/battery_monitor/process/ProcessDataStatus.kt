package com.example.battery_monitor.process

import android.annotation.SuppressLint
import android.app.Activity
import android.widget.TextView
import com.example.battery_monitor.R
import org.json.JSONObject
import kotlin.math.floor

class ProcessDataStatus(
    private val activity: Activity,
    private val addressMapper: (String) -> String = { addr -> addr }
) {

    private val addrTextView: TextView = activity.findViewById(R.id.statusAddrTextView)
    private val timeTextView: TextView = activity.findViewById(R.id.statusTimeTextView)
    private val envTempTextView: TextView = activity.findViewById(R.id.statusEnvTempTextView)
    private val batTempTextView: TextView = activity.findViewById(R.id.statusBatTempTextView)
    private val batTempValuesView: ProcessViewBatteryTemperature = activity.findViewById(R.id.statusBatTempValuesView)
    private val fanSpeedTextView: TextView = activity.findViewById(R.id.statusFanSpeedTextView)

    @SuppressLint("SetTextI18n")
    fun render(json: JSONObject) {
        activity.runOnUiThread {
            addrTextView.text = addressMapper (json.getString("addr"))
            timeTextView.text = json.getString("time")

            json.getJSONObject("tmp").optDouble("env", Double.NaN).let { env ->
                envTempTextView.text = if (!env.isNaN()) {
                    "%.1f°C (ext)".format(env)
                } else {
                    "--.-°C (ext)"
                }
            }
            json.getJSONObject("tmp").getJSONObject("bat").optDouble("avg", Double.NaN).let { avg ->
                if (!avg.isNaN()) {
                    val bat = json.getJSONObject("tmp").getJSONObject("bat")
                    batTempTextView.text = "%.1f°C (avg), %.1f°C (min), %.1f°C (max)".format(
                        bat.getDouble("avg"),
                        bat.getDouble("min"),
                        bat.getDouble("max")
                    )
                    val vat = bat.getJSONArray("val")
                    batTempValuesView.setTemperatureValues(
                        (0 until vat.length()).map { vat.getDouble(it).toFloat() }
                    )
                } else {
                    batTempTextView.text = "--.-°C (avg), --.-°C (min), --.-°C (max)"
                }
            }

            val fan = floor(100 * json.getInt("fan").toDouble() / 255).toInt()
            fanSpeedTextView.text = "$fan % (fan multiMap)"
        }
    }
}
