package com.example.battery_monitor

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.TextView
import org.json.JSONObject
import android.util.TypedValue

class MainActivity : Activity () {

    private val bluetoothPermissionsList = arrayOf (android.Manifest.permission.BLUETOOTH_SCAN, android.Manifest.permission.BLUETOOTH_CONNECT)
    private val bluetoothPermissionsCode = 1
    private var bluetoothPermissionsOkay = false;
    private val bluetoothManager: BluetoothManager by lazy {
        BluetoothManager (
            context = this,
            dataCallback = { jsonString -> bluetoothProcessReceivedData (jsonString) },
            statusCallback = { isConnected -> bluetoothUpdateConnectionStatus (isConnected) }
        )
    }

    //

    override fun onCreate (savedInstanceState: Bundle?) {
        super.onCreate (savedInstanceState)
        setContentView (R.layout.activity_main)
        bluetoothInitialise ()
    }
    override fun onDestroy () {
        super.onDestroy ()
        bluetoothTerminate ()
    }

    //

    override fun onCreateOptionsMenu (menu: Menu): Boolean {
        if (!EmulationTools.isEmulator ()) return false
        menuInflater.inflate (R.menu.menu_main, menu)
        return true
    }
    override fun onOptionsItemSelected (item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.inject_operational_data -> dataProcessor (EmulationTools.createTestDataOperational (this))
            R.id.inject_diagnostic_data -> dataProcessor (EmulationTools.createTestDataDiagnostic (this))
            else -> return super.onOptionsItemSelected (item)
        }
        return true
    }

    //

    override fun onRequestPermissionsResult (requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == bluetoothPermissionsCode) bluetoothPermissionsResponse (grantResults)
    }

    //

    private fun bluetoothInitialise () { if (!bluetoothPermissionsCheck ()) bluetoothPermissionsRequest () else bluetoothPermissionsOkay () }
    private fun bluetoothTerminate () { if (bluetoothPermissionsOkay) bluetoothManager.deviceDisconnect () }
    private fun bluetoothPermissionsCheck (): Boolean { return bluetoothPermissionsList.all { checkSelfPermission (it) == PackageManager.PERMISSION_GRANTED } }
    private fun bluetoothPermissionsRequest () { requestPermissions (bluetoothPermissionsList, bluetoothPermissionsCode) }
    private fun bluetoothPermissionsResponse (results: IntArray) {
        if (results.isNotEmpty () && results.all { it == PackageManager.PERMISSION_GRANTED }) bluetoothPermissionsOkay () else bluetoothPermissionsNotOkay ()
    }
    private fun bluetoothPermissionsOkay () {
        bluetoothPermissionsOkay = true
        bluetoothUpdateConnectionStatus (false)
        bluetoothManager.scan ()
    }
    private fun bluetoothPermissionsNotOkay () {
        bluetoothPermissionsOkay = false
        bluetoothUpdateConnectionStatus (false)
    }
    private fun bluetoothUpdateConnectionStatus (isConnected: Boolean) {
        dataConnection (bluetoothManager.isAvailable (), bluetoothPermissionsOkay, isConnected)
    }
    private fun bluetoothProcessReceivedData (jsonString: String) {
        try {
            dataProcessor (JSONObject (jsonString))
        } catch (e: Exception) {
            Log.e ("JSON", "JSON parse error", e)
        }
    }

    //

    private fun dataConnection (available: Boolean, permitted: Boolean, connected: Boolean) {
        val view: TextView = findViewById (R.id.connectionStatusTextView)
        val text = when {
            !available -> "Bluetooth not available"
            !permitted -> "Bluetooth not permitted"
            !connected -> "Bluetooth not connected"
            else -> "Bluetooth connected"
        }
        val color = getColor (if (connected) R.color.connected_color else R.color.disconnected_color)

        runOnUiThread {
            view.text = text;
            view.setTextColor (color)
        }
    }
    private fun dataProcessor (json: JSONObject) {
        when (val type = json.getString ("type")) {
            "data" -> dataRenderOperational (json)
            "diag" -> dataRenderDiagnostic (json)
            else -> Log.w ("JSON", "JSON type unknown: $type")
        }
    }
    private fun dataRenderOperational (json: JSONObject) {
        val timeTextView: TextView = findViewById (R.id.timeTextView)
        val envTempTextView: TextView = findViewById (R.id.envTempTextView)
        val batteryLabelTextView: TextView = findViewById (R.id.batteryLabelTextView)
        val batteryTempMinTextView: TextView = findViewById (R.id.batteryTempMinTextView)
        val batteryTempAvgTextView: TextView = findViewById (R.id.batteryTempAvgTextView)
        val batteryTempMaxTextView: TextView = findViewById (R.id.batteryTempMaxTextView)
        val batteryTempValuesView: BatteryTemperatureView = findViewById (R.id.batteryTempValuesView)
        val fanSpeedTextView: TextView = findViewById (R.id.fanSpeedTextView)
        val alarmsTextView: TextView = findViewById (R.id.alarmsTextView)

        runOnUiThread {
            timeTextView.text = formatTime (json.getLong ("time"))
            val environment = json.getJSONObject ("temperatures").getDouble ("environment");
            envTempTextView.text = "$environment°C"

            batteryLabelTextView.text = "Battery:"
            val batterypack = json.getJSONObject ("temperatures").getJSONObject ("batterypack")
            batteryTempAvgTextView.text = "Avg: ${batterypack.getDouble ("avg")}°C"
            batteryTempMinTextView.text = "Min: ${batterypack.getDouble ("min")}°C"
            batteryTempMaxTextView.text = "Max: ${batterypack.getDouble ("max")}°C"
            val values = batterypack.getJSONArray ("values")
            batteryTempValuesView.setTemperatureValues ((0 until values.length ()).map { values.getDouble (it).toFloat () })

            val fanspeed = json.getJSONObject ("fan").getInt ("fanspeed");
            fanSpeedTextView.text = "Fan: $fanspeed %"

            val alarms = json.getInt ("alarms")
            alarmsTextView.text = "Alarms: $alarms"
            alarmsTextView.setTextColor (getColor (if (alarms > 0) R.color.alarm_active_color else R.color.alarm_inactive_color))
        }
    }
    private fun dataRenderDiagnostic (json: JSONObject) {
        val view: TextView = findViewById (R.id.diagDataTextView)
        val text = json.toString (4)  // 4 spaces for indentation

        runOnUiThread {
            view.setTextSize (TypedValue.COMPLEX_UNIT_SP, 10f)
            view.text = text
        }
    }

    //

}