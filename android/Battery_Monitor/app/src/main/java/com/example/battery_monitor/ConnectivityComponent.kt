package com.example.battery_monitor

import android.os.Handler
import android.os.Looper
import android.util.Log

@Suppress("ReplaceNotNullAssertionWithElvisReturn")
abstract class ConnectivityComponent(
    protected open val tag: String,
    private val timer: Int = 0
) {
    protected val handler = Handler(Looper.getMainLooper())
    private var active: Boolean = false

    protected open fun onStart() {}
    protected open fun onStop() {}
    protected open fun onTimer(): Boolean {
        return false
    }

    private var runnable: Runnable? = null

    fun start() {
        if (!active) {
            Log.d(tag, "Start")
            onStart()
            active = true
            if (timer > 0) {
                runnable?.let {
                    handler.removeCallbacks(it)
                    runnable = null
                }
                runnable = Runnable {
                    if (active) {
                        if (onTimer())
                            handler.postDelayed(runnable!!, timer*1000L)
                    }
                }
                handler.postDelayed(runnable!!, timer*1000L)
            }
            //Log.d(tag, "Started")
        }
    }

    fun stop() {
        if (active) {
            Log.d(tag, "Stop")
            active = false
            runnable?.let {
                handler.removeCallbacks(it)
                runnable = null
            }
            onStop()
            //Log.d(tag, "Stopped")
        }
    }
}