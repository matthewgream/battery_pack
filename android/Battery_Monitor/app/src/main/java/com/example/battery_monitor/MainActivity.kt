package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.TextView
import org.json.JSONObject

import android.view.GestureDetector
import android.view.MotionEvent
import androidx.core.view.GestureDetectorCompat

class MainActivity : Activity () {

    private lateinit var bluetoothManager: BluetoothManager
    private lateinit var dataDiagnosticHandler: DataDiagnosticHandler
    private lateinit var dataOperationalHandler: DataOperationalHandler
    private lateinit var notificationsManager: NotificationsManager

    //

    override fun onCreate (savedInstanceState: Bundle?) {
        super.onCreate (savedInstanceState)
        setContentView (R.layout.activity_main)
        bluetoothManager = BluetoothManager (this,
            dataCallback = {
                data -> processDataReceived (JSONObject (data))
            },
            statusCallback = {
                processDataConnection (bluetoothManager.isAvailable (), bluetoothManager.isPermitted (), bluetoothManager.isConnected ())
            })
        dataDiagnosticHandler = DataDiagnosticHandler (this)
        dataOperationalHandler = DataOperationalHandler (this)
        notificationsManager = NotificationsManager(this)
        setupConnectionStatusDoubleTap ()
    }

    //

    override fun onDestroy () {
        super.onDestroy ()
        bluetoothManager.onDestroy ()
    }
    override fun onPause () {
        super.onPause ()
        bluetoothManager.onPause ()
    }
    override fun onResume () {
        super.onResume ()
        bluetoothManager.onResume ()
    }

    //

    @SuppressLint("ClickableViewAccessibility")
    private fun setupConnectionStatusDoubleTap() {
        val gestureDetector = GestureDetectorCompat(this, object : GestureDetector.SimpleOnGestureListener() {
            override fun onDoubleTap(e: MotionEvent): Boolean {
                bluetoothManager.restart ()
                return true
            }
        })
        val connectionStatusTextView: TextView = findViewById(R.id.connectionStatusTextView)
        connectionStatusTextView.setOnTouchListener { _, event ->
            gestureDetector.onTouchEvent(event)
            true
        }
    }

    //

    override fun onCreateOptionsMenu (menu: Menu): Boolean {
        if (!EmulationTools.isEmulator ()) return false
        menuInflater.inflate (R.menu.menu_main, menu)
        return true
    }
    override fun onOptionsItemSelected (item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.inject_operational_data -> processDataReceived (EmulationTools.createTestDataOperational (this))
            R.id.inject_diagnostic_data -> processDataReceived (EmulationTools.createTestDataDiagnostic (this))
            else -> return super.onOptionsItemSelected (item)
        }
        return true
    }

    //

    override fun onRequestPermissionsResult (requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        when (requestCode) {
            bluetoothManager.PERMISSIONS_CODE -> bluetoothManager.permissionsHandler (grantResults)
            notificationsManager.PERMISSIONS_CODE -> notificationsManager.permissionsHandler (grantResults)
            else -> super.onRequestPermissionsResult (requestCode, permissions, grantResults)
        }
    }

    //

    private fun processDataConnection (available: Boolean, permitted: Boolean, connected: Boolean) {
        val view: TextView = findViewById (R.id.connectionStatusTextView)
        val text = when {
            !available -> "Bluetooth not available"
            !permitted -> "Bluetooth not permitted"
            !connected -> "Bluetooth not connected"
            else -> "Bluetooth connected"
        }
        val color = getColor (if (connected) R.color.connected_color else R.color.disconnected_color)

        runOnUiThread {
            view.text = text
            view.setTextColor (color)
        }
    }
    private fun processDataReceived (json: JSONObject) {
        when (val type = json.getString ("type")) {
            "data" -> {
                dataOperationalHandler.render (json)
                notificationsManager.process (json.getString("alm"))
            }
            "diag" -> dataDiagnosticHandler.render (json)
            else -> Log.w ("JSON", "JSON type unknown: $type")
        }
    }

    //
}