package com.example.battery_monitor

import android.annotation.SuppressLint
//import android.content.Context
import android.os.Bundle
//import android.os.PowerManager
import android.util.Log
import android.widget.TextView
import org.json.JSONObject

import android.view.GestureDetector
import android.view.MotionEvent
import androidx.core.view.GestureDetectorCompat

class MainActivity : PermissionsAwareActivity () {

    private val connectionStatusTextView: TextView by lazy {
        findViewById (R.id.connectionStatusTextView)
    }

    private lateinit var bluetoothManager: BluetoothManager
    private lateinit var dataDiagnosticHandler: DataDiagnosticHandler
    private lateinit var dataOperationalHandler: DataOperationalHandler
    private lateinit var notificationsManager: NotificationsManager

    //

    override fun onCreate (savedInstanceState: Bundle?) {
        super.onCreate (savedInstanceState)
        setContentView (R.layout.activity_main)
        bluetoothManager = BluetoothManager (this,
            dataCallback = { data -> processDataReceived (JSONObject (data)) },
            statusCallback = { processDataConnection (bluetoothManager.isAvailable (), bluetoothManager.isPermitted (), bluetoothManager.isConnected ()) })
        dataDiagnosticHandler = DataDiagnosticHandler (this)
        dataOperationalHandler = DataOperationalHandler (this)
        notificationsManager = NotificationsManager(this)
        setupConnectionStatusDoubleTap ()
    }

    override fun onDestroy () {
        Log.d ("Main", "onDestroy")
        super.onDestroy ()
        bluetoothManager.onDestroy ()
    }
    override fun onPause () {
        Log.d ("Main", "onPause")
        super.onPause ()
        bluetoothManager.onPause ()
    }
    override fun onResume () {
        Log.d ("Main", "onResume")
        super.onResume ()
        bluetoothManager.onResume ()
    }

    //

    @SuppressLint("ClickableViewAccessibility")
    private fun setupConnectionStatusDoubleTap () {
        val detector = GestureDetectorCompat (this, object : GestureDetector.SimpleOnGestureListener () {
            override fun onDoubleTap (e: MotionEvent): Boolean {
                bluetoothManager.onDoubleTap ()
                return true
            }
        })
        connectionStatusTextView.setOnTouchListener { _, event ->
            detector.onTouchEvent (event)
            true
        }
    }

    private fun processDataConnection (available: Boolean, permitted: Boolean, connected: Boolean) {
        val text = when {
            !available -> "Bluetooth not available"
            !permitted -> "Bluetooth not permitted"
            !connected -> "Bluetooth not connected"
            else -> "Bluetooth connected"
        }
        val color = getColor (if (connected) R.color.connected_color else R.color.disconnected_color)

        runOnUiThread {
            connectionStatusTextView.text = text
            connectionStatusTextView.setTextColor (color)
        }
    }
    private fun processDataReceived (json: JSONObject) {
        when (val type = json.getString ("type")) {
            "data" -> {
                dataOperationalHandler.render (json)
                notificationsManager.process (json.getString ("alm"))
            }
            "diag" -> dataDiagnosticHandler.render (json)
            else -> Log.w ("Main", "JSON type unknown: $type")
        }
    }

//    fun canPerformBluetoothOperations (): Boolean {
//        val powerManager = getSystemService (Context.POWER_SERVICE) as PowerManager
//        return !powerManager.isPowerSaveMode && resources.getBoolean (R.bool.config_bluetooth_allowed_while_driving)
//    }
//    fun registerDozeListener () {
//        val powerManager = getSystemService (Context.POWER_SERVICE) as PowerManager
//        powerManager.addThermalStatusListener { status ->
//            when (status) {
//                PowerManager.THERMAL_STATUS_SEVERE,
//                PowerManager.THERMAL_STATUS_CRITICAL -> pause ()
//                else -> resume ()
//            }
//        }
//    }

}