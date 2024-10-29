package com.example.battery_monitor

import android.os.Bundle
import android.util.Log
import com.example.battery_monitor.connect.BluetoothDeviceManager
import com.example.battery_monitor.connect.CloudMqttDeviceManager
import com.example.battery_monitor.connect.ConnectManager
import com.example.battery_monitor.connect.ConnectStatusView
import com.example.battery_monitor.connect.WebSocketDeviceManager
import com.example.battery_monitor.process.ProcessManager
import com.example.battery_monitor.utility.NotificationsManager
import com.example.battery_monitor.utility.PermissionsAwareActivity
import com.example.battery_monitor.utility.PowerstateManager

class MainActivity : PermissionsAwareActivity() {

    class Config (
        val name: String
    )

    private val secrets = Secrets ()
    private val config = Config(
        name = "batterymonitor"
    )

    //

    private val power by lazy {
        PowerstateManager(this,
            onPowerSave = { onPowerSave() },
            onPowerBack = { onPowerBack() }
        )
    }
    private val notifier by lazy {
        NotificationsManager(this,
            NotificationsManager.Config (
                name = this.getString(R.string.notifications_name),
                description = this.getString(R.string.notifications_description)
            ), R.drawable.ic_notification)
    }
    private val processor by lazy {
        ProcessManager("Process", this, notifier,
            addressMapper = { addr ->
            when (addr) {
                secrets.DEVICE_ADDR -> "${secrets.DEVICE_NAME} ($addr)"
                else -> addr
            }
        })
    }
    private val connector by lazy {
        ConnectManager ("Connnect", this,
            config.name,
            ConnectManager.Config (
                BluetoothDeviceManager.Config(
                    deviceName ="BatteryMonitor",
                ),
                WebSocketDeviceManager.Config(
                    serviceName = "BatteryMonitor",
                    serviceType = "_ws._tcp.",
                ),
                CloudMqttDeviceManager.Config(
                    host = secrets.MQTT_HOST,
                    port = secrets.MQTT_PORT,
                    user = secrets.MQTT_USER,
                    pass = secrets.MQTT_PASS,
                    root = "BatteryMonitor"
                ),
            ),
            findViewById(R.id.connectStatusView),
            ConnectStatusView.Config(
                layout = R.layout.connect_status,
                views = ConnectStatusView.Config.Views(
                    direct = R.id.iconDirect,
                    local = R.id.iconLocal,
                    cloud = R.id.iconCloud
                ),
                colors = ConnectStatusView.Config.Colors(
                    disabled = R.color.ic_connect_disabled,
                    disconnected = R.color.ic_connect_disconnected,
                    connectedStandby = R.color.ic_connect_connected_standby,
                    connectedActive = R.color.ic_connect_connected_active
                )
            ),
            onReceiveData = { json -> 
                processor.processDataReceived(json)
            },
            addressExtractor = { json ->
                if (json.getString("type") == "data" && json.has("addr")) {
                    json.getString("addr")
                } else {
                    null
                }
            }
        )
    }

    //

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.d("Main", "onCreate")
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        connector.onCreate()
        power.onCreate()
    }
    override fun onDestroy() {
        Log.d("Main", "onDestroy")
        super.onDestroy()
        connector.onDestroy()
    }
    override fun onPause() {
        Log.d("Main", "onPause")
        super.onPause()
        connector.onPause()
    }
    override fun onResume() {
        Log.d("Main", "onResume")
        super.onResume()
        connector.onResume()
    }
    private fun onPowerSave() {
        Log.d("Main", "onPowerSave")
        connector.onPowerSave()
    }
    private fun onPowerBack() {
        Log.d("Main", "onPowerBack")
        connector.onPowerBack()
    }
}