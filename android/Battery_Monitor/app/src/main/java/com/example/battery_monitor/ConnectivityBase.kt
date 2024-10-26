package com.example.battery_monitor

enum class ConnectivityType {
    LOCAL,
    NETWORK,
    CLOUD
}
data class ConnectivityStatus(
    val permitted: Boolean,
    val available: Boolean,
    val connected: Boolean,
    val standby: Boolean
)
