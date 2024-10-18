package com.example.battery_monitor

import android.os.Handler
import android.os.Looper
import android.util.Log

abstract class ConnectivityComponent (protected open val tag: String) {

    protected val handler = Handler (Looper.getMainLooper ())
    private var active: Boolean = false

    protected open fun onStart () {}
    protected open fun onStop () {}
    protected open fun onTimer () : Boolean { return false }

    open val timer: Long = 0L

    private var runnable : Runnable ?= null

    fun start () {
        if (!active) {
            onStart ()
            active = true
            if (timer > 0) {
                runnable?.let {
                    handler.removeCallbacks (it)
                    runnable = null
                }
                runnable = Runnable {
                    if (active) {
                        if (onTimer ())
                            handler.postDelayed (runnable!!, timer)
                    }
                }
                handler.postDelayed (runnable!!, timer)
            }
            Log.d (tag, "Started")
        }
    }

    fun stop () {
        if (active) {
            active = false
            runnable?.let {
                handler.removeCallbacks (it)
                runnable = null
            }
            onStop ()
            Log.d (tag, "Stopped")
        }
    }
}