package com.example.battery_monitor

import android.os.Handler
import android.os.Looper
import android.util.Log

@Suppress("ReplaceNotNullAssertionWithElvisReturn")
abstract class ConnectivityComponent(
    protected open val tag: String,
    private val timer: Int = 0
) {
    protected val handler : Handler = Handler(Looper.getMainLooper())
    private var active = Activable ()

    protected open fun onStart() {}
    protected open fun onStop() {}
    protected open fun onTimer(): Boolean {
        return false
    }

    private var runnable: Runnable? = null

    fun start() {
        if (active.toActive ()) {
            Log.d(tag, "Start")
            onStart()
            if (timer > 0) {
                runnable?.let {
                    handler.removeCallbacks(it)
                    runnable = null
                }
                runnable = Runnable {
                    if (active.isActiveNow)
                        if (onTimer())
                            handler.postDelayed(runnable!!, timer*1000L)
                }
                handler.postDelayed(runnable!!, timer*1000L)
            }
        }
    }

    fun stop() {
        if (active.toInactive()) {
            Log.d(tag, "Stop")
            runnable?.let {
                handler.removeCallbacks(it)
                runnable = null
            }
            onStop()
        }
    }
}