package com.example.battery_monitor.utility

import android.os.Looper
import kotlin.reflect.KProperty

class MainThreadInitializer<T>(private val initializer: () -> T) {
    private var value: T? = null
    operator fun getValue(thisRef: Any?, property: KProperty<*>): T {
        if (value == null) {
            check(Looper.myLooper() == Looper.getMainLooper()) { "${property.name} must be initialized on main thread" }
            value = initializer()
        }
        return value!!
    }
}

fun <T> mainThreadInit(initializer: () -> T): MainThreadInitializer<T> = MainThreadInitializer(initializer)
