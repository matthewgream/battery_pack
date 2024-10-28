package com.example.battery_monitor

enum class ConnectivityType {
    DIRECT,
    LOCAL,
    CLOUD
}
data class ConnectivityStatus(
    val available: Boolean,
    val permitted: Boolean,
    val connected: Boolean
)
