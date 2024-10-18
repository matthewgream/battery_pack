package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import android.widget.TextView
import org.json.JSONObject

import android.view.GestureDetector
import android.view.MotionEvent

class MainActivity : PermissionsAwareActivity () {

    private val connectionStatusTextView: TextView by lazy {
        findViewById (R.id.connectionStatusTextView)
    }
    private var powerSaveState : Boolean = false

    private lateinit var notificationsManager: NotificationsManager
    private lateinit var dataProcessor: DataProcessor
    private lateinit var bluetoothManager: BluetoothManager
    private lateinit var networkManager: NetworkManager

    //

    override fun onCreate (savedInstanceState: Bundle?) {
        super.onCreate (savedInstanceState)
        setContentView (R.layout.activity_main)
        notificationsManager = NotificationsManager (this)
        dataProcessor = DataProcessor (this, notificationsManager)
        bluetoothManager = BluetoothManager (this,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { updateConnectionStatus () })
        networkManager = NetworkManager(this,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { updateConnectionStatus () }
        )
        setupConnectionStatusDoubleTap ()
        setupPowerSave ()
    }

    override fun onDestroy () {
        Log.d ("Main", "onDestroy")
        super.onDestroy ()
        bluetoothManager.onDestroy ()
        networkManager.onDestroy()
    }
    override fun onPause () {
        Log.d ("Main", "onPause")
        super.onPause ()
        bluetoothManager.onPause ()
        networkManager.onPause ()
    }
    override fun onResume () {
        Log.d ("Main", "onResume")
        super.onResume ()
        bluetoothManager.onResume ()
        networkManager.onResume ()
    }
    private fun onPowerSave() {
        Log.d ("Main", "onPowerSave")
        bluetoothManager.onPowerSave()
        networkManager.onPowerSave()
    }

    //

    private fun setupPowerSave () {
        val powerManager = getSystemService (Context.POWER_SERVICE) as PowerManager
        powerSaveState = powerManager.isPowerSaveMode
        powerManager.addThermalStatusListener { status ->
            when (status) {
                PowerManager.THERMAL_STATUS_SEVERE,
                PowerManager.THERMAL_STATUS_CRITICAL ->
                    if (!powerSaveState) {
                        powerSaveState = true
                        onPowerSave()
                    }
                else ->
                    if (powerSaveState) {
                        powerSaveState = false
                        onPowerSave()
                }
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupConnectionStatusDoubleTap () {
        val detector = GestureDetector (this, object : GestureDetector.SimpleOnGestureListener () {
            override fun onDoubleTap (e: MotionEvent): Boolean {
                bluetoothManager.onDoubleTap ()
                networkManager.onDoubleTap ()
                return true
            }
        })
        connectionStatusTextView.setOnTouchListener { _, event ->
            detector.onTouchEvent (event)
            true
        }
    }

    private fun updateConnectionStatus() {
        val bluetoothConnected = bluetoothManager.isConnected ()
        val networkConnected = networkManager.isConnected ()
        val text = when {
            bluetoothConnected && networkConnected -> "Bluetooth and Wi-Fi connected"
            bluetoothConnected -> "Bluetooth connected"
            networkConnected -> "Wi-Fi connected"
            !bluetoothManager.isAvailable() && !networkManager.isAvailable() -> "No connectivity available"
            !bluetoothManager.isPermitted() || !networkManager.isPermitted() -> "Permissions not granted"
            else -> "Not connected"
        }
        val color = getColor(if (bluetoothConnected || networkConnected) R.color.connected_color else R.color.disconnected_color)
        runOnUiThread {
            connectionStatusTextView.text = text
            connectionStatusTextView.setTextColor(color)
        }
    }
}