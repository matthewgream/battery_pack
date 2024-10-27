package com.example.battery_monitor

class Activable {
    private var isActive = false
    fun toActive(): Boolean {
        if (!isActive) {
            isActive = true
            return true
        }
        return false
    }
    fun toInactive(): Boolean {
        if (isActive) {
            isActive = false
            return true
        }
        return false
    }
    val isActiveNow: Boolean
        get() = isActive
}

