package com.example.battery_monitor

enum class ConnectivityType {
    DIRECT,
    LOCAL,
    CLOUD
}
enum class ConnectivityState {
    Connecting,
    Connected,
    Disconnected
}
data class ConnectivityStatus(
    val available: Boolean,
    val permitted: Boolean,
    val connected: Boolean
)
