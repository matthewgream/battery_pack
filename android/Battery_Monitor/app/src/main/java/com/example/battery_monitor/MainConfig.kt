package com.example.battery_monitor

import android.app.Activity
import java.util.UUID

//

class ApplicationConfig (
    val name: String = "batterymonitor"
)
class BluetoothDeviceHandlerConfig (
    val deviceName: String = "BatteryMonitor",
    val serviceUuid: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b"),
    val characteristicUuid: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
    val connectionScanDelay : Long = 5000L,
    val connectionScanPeriod : Long = 30000L,
    val connectionActiveTimeout: Long = 30000L,
    val connectionMtu: Int = 517
)
class CloudMqttDeviceConfig (
    val host: String = SECRET_MQTT_HOST,
    val port: Int = SECRET_MQTT_PORT,
    val user: String? = SECRET_MQTT_USER,
    val pass: String? = SECRET_MQTT_PASS,
    val topic: String = "BatteryMonitor",
    val connectionActiveTimeout: Long = 30000L,
    val connectionActiveCheck: Long = 15000L
)
class WebSocketDeviceConfig (
    val serviceName: String = "BatteryMonitor",
    val serviceType: String = "_ws._tcp.",
    val connectionScanDelay: Long = 5000L,
    val connectionScanPeriod: Long = 30000L,
    val connectionActiveTimeout: Long = 30000L,
    val connectionActiveCheck: Long = 15000L
)
class NotificationsManagerConfig(activity: Activity) {
    val name = activity.getString(R.string.notification_channel_name)
    val description = activity.getString(R.string.notification_channel_description)
    val title = activity.getString(R.string.notification_channel_title)
    val content = activity.getString(R.string.notification_channel_content)
}

//