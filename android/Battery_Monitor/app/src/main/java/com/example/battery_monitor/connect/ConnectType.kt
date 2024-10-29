package com.example.battery_monitor.connect

enum class ConnectType {
    DIRECT,
    LOCAL,
    CLOUD
}
enum class ConnectState {
    Connecting,
    Connected,
    Disconnected
}
data class ConnectStatus(
    val available: Boolean,
    val permitted: Boolean,
    val connected: Boolean
)
