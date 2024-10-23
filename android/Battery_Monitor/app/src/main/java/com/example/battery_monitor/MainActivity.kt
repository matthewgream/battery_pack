package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import org.json.JSONObject
import java.time.Instant
import java.time.format.DateTimeFormatter

class ConnectionInfo(private val activity: Activity) {
    private val prefs: SharedPreferences = activity.getSharedPreferences("ConnectionInfo", Context.MODE_PRIVATE)

    private val appName = "batterymonitor"
    private val appVersion = try {
        activity.packageManager.getPackageInfo(activity.packageName, PackageManager.PackageInfoFlags.of(0)).versionName
    } catch (e: PackageManager.NameNotFoundException) {
        "?.?.?"
    }
    private val appPlatform = "android${Build.VERSION.SDK_INT}"
    private val appDevice = "${Build.MANUFACTURER} ${Build.MODEL}"

    var deviceAddress: String = ""
        private set

    init {
        // Load saved device address on creation
        deviceAddress = prefs.getString("device_address", "") ?: ""
    }

    fun updateDeviceAddress(newAddress: String) {
        deviceAddress = newAddress
        prefs.edit().putString("device_address", newAddress).apply()
    }

    fun toJsonString(): String {
        return JSONObject().apply {
            put("type", "info")
            put("time", DateTimeFormatter.ISO_INSTANT.format(Instant.now()))
            put("info", "$appName-custom-$appPlatform-v$appVersion ($appDevice)")
        }.toString()
    }
}

class MainActivity : PermissionsAwareActivity() {

    private val connectionInfo by lazy { ConnectionInfo (this) }
    private var powerSaveState: Boolean = false

    private val connectivityStatusView: DataViewConnectivityStatus by lazy {
        findViewById(R.id.connectivityStatusView)
    }
    private val bluetoothManager: BluetoothManager by lazy {
        BluetoothManager(this, connectionInfo,
            dataCallback = { data ->
                processBluetoothAddr(data)
                dataProcessor.processDataReceived(JSONObject(data))
            },
            statusCallback = { updateConnectionStatus() })
    }
    private val networkManager: NetworkManager by lazy {
        NetworkManager(this, connectionInfo,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { updateConnectionStatus() })
    }
    private val cloudManager: CloudManager by lazy {
        CloudManager(this, connectionInfo,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { updateConnectionStatus() })
    }
    private val notificationsManager: NotificationsManager by lazy {
        NotificationsManager(this)
    }
    private lateinit var dataProcessor: DataProcessor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        dataProcessor = DataProcessor(this,
            notificationsManager = notificationsManager)

        setupConnectionStatusDoubleTap()
        setupPowerSave()
    }

    override fun onDestroy() {
        Log.d("Main", "onDestroy")
        super.onDestroy()
        bluetoothManager.onDestroy()
        networkManager.onDestroy()
        cloudManager.onDestroy()
    }

    override fun onPause() {
        Log.d("Main", "onPause")
        super.onPause()
        bluetoothManager.onSuspend(true)
        networkManager.onSuspend(true)
        cloudManager.onSuspend(true)
    }

    override fun onResume() {
        Log.d("Main", "onResume")
        super.onResume()
        bluetoothManager.onSuspend(false)
        networkManager.onSuspend(false)
        cloudManager.onSuspend(false)
    }

    private fun onPowerSave(enabled: Boolean) {
        Log.d("Main", "onPowerSave($enabled)")
        bluetoothManager.onPowerSave(enabled)
        networkManager.onPowerSave(enabled)
        cloudManager.onPowerSave(enabled)
    }

    private fun onDoubleTap() {
        Log.d("Main", "onDoubleTap")
        bluetoothManager.onDoubleTap()
        networkManager.onDoubleTap()
        cloudManager.onDoubleTap()
    }

    private fun setupPowerSave() {
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        powerSaveState = powerManager.isPowerSaveMode
        powerManager.addThermalStatusListener { status ->
            when (status) {
                PowerManager.THERMAL_STATUS_SEVERE,
                PowerManager.THERMAL_STATUS_CRITICAL ->
                    if (!powerSaveState) {
                        powerSaveState = true
                        onPowerSave(true)
                    }
                else ->
                    if (powerSaveState) {
                        powerSaveState = false
                        onPowerSave(false)
                    }
            }
        }
    }
    private fun processBluetoothAddr(data: String) {
        try {
            val json = JSONObject(data)
            if (json.getString("type") == "data" && json.has("addr")) {
                val newAddr = json.getString("addr")
                if (newAddr != connectionInfo.deviceAddress) {
                    connectionInfo.updateDeviceAddress(newAddr)
                    Log.d("MainActivity", "Device address changed, triggering network and cloud connection attempts")
                    networkManager.onDoubleTap()
                    cloudManager.onDoubleTap()
                }
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "Error processing bluetooth address", e)
        }
    }
    private fun setupConnectionStatusDoubleTap() {
        connectivityStatusView.setOnDoubleTapListener {
            onDoubleTap ()
        }
    }
    private fun updateConnectionStatus() {
        connectivityStatusView.updateStatus (
            bluetoothPermitted = bluetoothManager.isPermitted(), bluetoothAvailable = bluetoothManager.isAvailable(), bluetoothConnected = bluetoothManager.isConnected(),
            networkPermitted = networkManager.isPermitted(), networkAvailable = networkManager.isAvailable(), networkConnected = networkManager.isConnected(),
            cloudPermitted = cloudManager.isPermitted() , cloudAvailable = cloudManager.isAvailable(), cloudConnected = cloudManager.isConnected(),
        )
    }
}