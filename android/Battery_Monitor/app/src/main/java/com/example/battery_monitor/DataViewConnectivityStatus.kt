package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.LayoutInflater
import android.view.MotionEvent
import android.widget.LinearLayout
import android.widget.ImageView
import android.os.Handler
import android.os.Looper

class DataViewConnectivityStatus @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : LinearLayout(context, attrs, defStyleAttr) {

    private val uiHandler = Handler(Looper.getMainLooper())
    private lateinit var doubleTapListener: () -> Unit
    private lateinit var gestureDetector: GestureDetector

    private val bluetoothIcon: ImageView
    private val networkIcon: ImageView
    private val cloudIcon: ImageView

    private enum class IconState {
        DISABLED, // Grey - No permissions or not available
        DISCONNECTED, // Red - Available but not connected
        CONNECTED, // Green - Connected and active
        STANDBY // Orange - Connected but inactive (for future use)
    }

    init {
        orientation = VERTICAL
        LayoutInflater.from(context).inflate(R.layout.view_connectivity_status, this, true)

        bluetoothIcon = findViewById(R.id.bluetoothIcon)
        networkIcon = findViewById(R.id.networkIcon)
        cloudIcon = findViewById(R.id.cloudIcon)

        elevation = 4f
    }

    private fun updateIconState(icon: ImageView, state: IconState) {
        val color = when (state) {
            IconState.DISABLED -> context.getColor(R.color.icon_disabled)
            IconState.DISCONNECTED -> context.getColor(R.color.icon_disconnected)
            IconState.CONNECTED -> context.getColor(R.color.icon_connected)
            IconState.STANDBY -> context.getColor(R.color.icon_standby)
        }
        icon.setColorFilter(color)
    }

    fun updateStatus(
        bluetoothConnected: Boolean, networkConnected: Boolean, cloudConnected: Boolean,
        bluetoothAvailable: Boolean, networkAvailable: Boolean, cloudAvailable: Boolean,
        bluetoothPermitted: Boolean, networkPermitted: Boolean, cloudPermitted: Boolean
    ) {
        uiHandler.post {
            val bluetoothState = when {
                !bluetoothPermitted -> IconState.DISABLED
                !bluetoothAvailable -> IconState.DISABLED
                !bluetoothConnected -> IconState.DISCONNECTED
                else -> IconState.CONNECTED
            }
            updateIconState(bluetoothIcon, bluetoothState)

            val networkState = when {
                !networkPermitted -> IconState.DISABLED
                !networkAvailable -> IconState.DISABLED
                !networkConnected -> IconState.DISCONNECTED
                else -> IconState.CONNECTED
            }
            updateIconState(networkIcon, networkState)

            val cloudState = when {
                !cloudPermitted -> IconState.DISABLED
                !cloudAvailable -> IconState.DISABLED
                !cloudConnected -> IconState.DISCONNECTED
                else -> IconState.CONNECTED
            }
            updateIconState(cloudIcon, cloudState)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    fun setOnDoubleTapListener(listener: () -> Unit) {
        this.doubleTapListener = listener
        gestureDetector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
            override fun onDoubleTap(e: MotionEvent): Boolean {
                doubleTapListener.invoke()
                return true
            }
        })

        setOnTouchListener { _, event ->
            gestureDetector.onTouchEvent(event)
            true
        }
    }
}