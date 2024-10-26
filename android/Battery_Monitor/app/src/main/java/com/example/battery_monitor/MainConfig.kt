package com.example.battery_monitor

import android.app.Activity
import java.util.UUID

//

class MainConfig (
    val name: String = "batterymonitor"
)
class BluetoothDeviceConfig (
    val deviceName: String = "BatteryMonitor",
    val serviceUuid: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b"),
    val characteristicUuid: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
    val connectionScanDelay : Int = 5,
    val connectionScanPeriod : Int = 30,
    val connectionActiveCheck: Int = 15,
    val connectionActiveTimeout: Int = 30,
    val connectionMtu: Int = 517
)
class CloudMqttDeviceConfig (
    val host: String = SECRET_MQTT_HOST,
    val port: Int = SECRET_MQTT_PORT,
    val user: String? = SECRET_MQTT_USER,
    val pass: String? = SECRET_MQTT_PASS,
    val topic: String = "BatteryMonitor",
    val connectionActiveCheck: Int = 15,
    val connectionActiveTimeout: Int = 30
)
class WebSocketDeviceConfig (
    val serviceName: String = "BatteryMonitor",
    val serviceType: String = "_ws._tcp.",
    val connectionScanDelay: Int = 5,
    val connectionScanPeriod: Int = 30,
    val connectionActiveCheck: Int = 15,
    val connectionActiveTimeout: Int = 30
)
class NotificationsConfig(activity: Activity) {
    val name: String = activity.getString(R.string.notifications_name)
    val description: String = activity.getString(R.string.notifications_description)
}

//