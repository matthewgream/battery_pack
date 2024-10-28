package com.example.battery_monitor

class Activable(
    initial: Boolean = false
) {
    private var _isActive = initial
    fun toActive(): Boolean {
        if (!_isActive) {
            _isActive = true
            return true
        }
        return false
    }
    fun toInactive(): Boolean {
        if (_isActive) {
            _isActive = false
            return true
        }
        return false
    }
    var isActive: Boolean
        get() = _isActive
        set(value) { _isActive = value }
}

