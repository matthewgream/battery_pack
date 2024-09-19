package com.example.battery_monitor

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.TextView
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.*
import android.util.TypedValue

class MainActivity : Activity() {

    private val PERMISSION_REQUEST_CODE = 1
    private val bluetoothManager: BluetoothManager by lazy {
        BluetoothManager(
            context = this,
            dataCallback = { jsonString -> bluetoothProcessReceivedData(jsonString) },
            statusCallback = { isConnected -> bluetoothUpdateConnectionStatus(isConnected) }
        )
    }
    private var bluetoothPermitted = false;

    // Connection Status View
    private val connectionStatusTextView: TextView by lazy { findViewById(R.id.connectionStatusTextView) }
    // Operational Data Views
    private val timeTextView: TextView by lazy { findViewById(R.id.timeTextView) }
    private val envTempTextView: TextView by lazy { findViewById(R.id.envTempTextView) }
    private val batteryLabelTextView: TextView by lazy { findViewById(R.id.batteryLabelTextView) }
    private val batteryTempMinTextView: TextView by lazy { findViewById(R.id.batteryTempMinTextView) }
    private val batteryTempAvgTextView: TextView by lazy { findViewById(R.id.batteryTempAvgTextView) }
    private val batteryTempMaxTextView: TextView by lazy { findViewById(R.id.batteryTempMaxTextView) }
    private val batteryTempValuesView: BatteryTemperatureView by lazy { findViewById(R.id.batteryTempValuesView) }
    private val fanSpeedTextView: TextView by lazy { findViewById(R.id.fanSpeedTextView) }
    private val alarmsTextView: TextView by lazy { findViewById(R.id.alarmsTextView) }
    // Diagnostic Data View
    private val diagDataTextView: TextView by lazy { findViewById(R.id.diagDataTextView) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        bluetoothInitialise()
    }

    private fun bluetoothInitialise() {
        if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED ||
            checkSelfPermission(android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ), PERMISSION_REQUEST_CODE)
        } else {
            bluetoothPermitted = true;
            bluetoothUpdateConnectionStatus (false)
            bluetoothManager.scan()
        }
    }

    private fun bluetoothUpdateConnectionStatus(isConnected: Boolean) {
        runOnUiThread {
            connectionStatusTextView.text = when {
                !bluetoothPermitted -> "Not permitted"
                isConnected -> "Connected"
                else -> "Disconnected"
            }
            connectionStatusTextView.setTextColor (getColor(if (isConnected) R.color.connected_color else R.color.disconnected_color))
        }
    }

    private fun bluetoothProcessReceivedData(jsonString: String) {
        try {
            val json = JSONObject(jsonString)
            when (json.getString("type")) {
                "data" -> updateOperationalUI(json)
                "diag" -> updateDiagnosticUI(json)
                else -> Log.w("JSON", "Unknown JSON type received")
            }
        } catch (e: Exception) {
            Log.e("JSON", "Error parsing JSON", e)
        }
    }

    private fun updateOperationalUI(json: JSONObject) {
        runOnUiThread {
            timeTextView.text = "Time: ${formatTime(json.getLong("time"))}"
            envTempTextView.text = "Temp: ${json.getJSONObject("temperatures").getDouble("environment")}°C"

            batteryLabelTextView.text = "Battery:"
            val batterypack = json.getJSONObject("temperatures").getJSONObject("batterypack")
            batteryTempAvgTextView.text = "Avg: ${batterypack.getDouble("avg")}°C"
            batteryTempMinTextView.text = "Min: ${batterypack.getDouble("min")}°C"
            batteryTempMaxTextView.text = "Max: ${batterypack.getDouble("max")}°C"

            val values = batterypack.getJSONArray("values")
            val temperatureValues = (0 until values.length()).map { values.getDouble(it).toFloat() }
            batteryTempValuesView.setTemperatureValues(temperatureValues)

            fanSpeedTextView.text = "Fan: ${json.getJSONObject("fan").getInt("fanspeed")} %"

            val alarms = json.getInt("alarms")
            alarmsTextView.text = "Alarms: $alarms"
            alarmsTextView.setTextColor(if (alarms > 0) getColor(R.color.alarm_active_color) else getColor(R.color.alarm_inactive_color))
        }
    }

    private fun updateDiagnosticUI(json: JSONObject) {
        runOnUiThread {
            diagDataTextView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            diagDataTextView.text = json.toString(4)  // 4 spaces for indentation
        }
    }

    private fun formatTime(timestamp: Long): String {
        return SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date(timestamp * 1000))
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        return if (EmulationManager.isEmulator()) {
            menuInflater.inflate(R.menu.main_menu, menu)
            true
        } else {
            false
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.inject_operational_data -> {
                bluetoothProcessReceivedData (EmulationManager.createTestOperationalData(this))
                true
            }
            R.id.inject_diagnostic_data -> {
                bluetoothProcessReceivedData (EmulationManager.createTestDiagnosticData(this))
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                bluetoothPermitted = true
                bluetoothUpdateConnectionStatus (false)
                bluetoothManager.scan()
            } else {
                bluetoothUpdateConnectionStatus (false)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        bluetoothManager.deviceDisconnect()
    }
}