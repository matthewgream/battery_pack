package com.example.battery_monitor

import android.os.Handler
import android.os.Looper
import android.util.Log

@Suppress("ReplaceNotNullAssertionWithElvisReturn")
abstract class ConnectivityComponent(
    protected open val tag: String,
    private val timerPeriod: Int = 0
) {
    protected val handler : Handler = Handler(Looper.getMainLooper())
    private var active = Activable ()

    protected open fun onStart() {}
    protected open fun onStop() {}
    protected open fun onTimer(): Boolean {
        return false
    }

    private var timerRunnable: Runnable? = null
    private fun timerStart() {
        timerRunnable = Runnable {
            if (active.isActiveNow)
                if (onTimer())
                    handler.postDelayed(timerRunnable!!, timerPeriod*1000L)
        }
        handler.postDelayed(timerRunnable!!, timerPeriod*1000L)
    }
    private fun timerStop() {
        timerRunnable?.let {
            handler.removeCallbacks(it)
            timerRunnable = null
        }
    }

    fun start() {
        if (active.toActive()) {
            Log.d(tag, "Start")
            onStart()
            if (timerPeriod > 0) {
                timerStop()
                timerStart()
            }
        }
    }
    fun stop() {
        if (active.toInactive()) {
            Log.d(tag, "Stop")
            timerStop()
            onStop()
        }
    }
}