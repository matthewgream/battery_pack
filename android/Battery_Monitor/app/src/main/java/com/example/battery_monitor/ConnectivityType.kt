package com.example.battery_monitor

enum class ConnectivityType {
    DIRECT,
    LOCAL,
    CLOUD
}
data class ConnectivityStatus(
    val permitted: Boolean,
    val available: Boolean,
    val connected: Boolean
)
