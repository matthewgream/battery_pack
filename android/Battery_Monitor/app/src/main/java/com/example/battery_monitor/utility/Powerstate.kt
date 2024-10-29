package com.example.battery_monitor.utility

import android.content.Context
import android.os.PowerManager

class PowerstateManager(
    context: Context,
    private val onPowerSave: () -> Unit,
    private val onPowerBack: () -> Unit
) {
    private var state = Activable()
    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager

    fun onCreate() {
        state.isActive = powerManager.isPowerSaveMode
        powerManager.addThermalStatusListener { status ->
            when (status) {
                PowerManager.THERMAL_STATUS_SEVERE,
                PowerManager.THERMAL_STATUS_CRITICAL ->
                    if (state.toActive())
                        onPowerSave()
                else ->
                    if (state.toInactive())
                        onPowerBack()
            }
        }
    }
}