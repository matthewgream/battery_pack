package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.LayoutInflater
import android.view.MotionEvent
import android.widget.ImageView
import android.widget.LinearLayout

class DataViewConnectivityStatus @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : LinearLayout(context, attrs, defStyleAttr) {

    private val uiHandler = Handler(Looper.getMainLooper())
    private lateinit var doubleTapListener: () -> Unit
    private lateinit var gestureDetector: GestureDetector

    private val localIcon: ImageView
    private val networkIcon: ImageView
    private val cloudIcon: ImageView

    private enum class IconState {
        DISABLED, // Grey - No permissions or not available
        DISCONNECTED, // Red - Available but not connected
        CONNECTED_STANDBY,// Orange - Connected but inactive (for future use)
        CONNECTED_ACTIVE // Green - Connected and active
    }

    init {
        orientation = VERTICAL
        LayoutInflater.from(context).inflate(R.layout.view_connectivity_status, this, true)

        localIcon = findViewById(R.id.localIcon)
        networkIcon = findViewById(R.id.networkIcon)
        cloudIcon = findViewById(R.id.cloudIcon)

        elevation = 4f
    }

    private fun updateIconState(icon: ImageView, state: IconState) {
        val color = when (state) {
            IconState.DISABLED -> context.getColor(R.color.icon_disabled)
            IconState.DISCONNECTED -> context.getColor(R.color.icon_disconnected)
            IconState.CONNECTED_STANDBY -> context.getColor(R.color.icon_connected_standby)
            IconState.CONNECTED_ACTIVE -> context.getColor(R.color.icon_connected_active)
        }
        icon.setColorFilter(color)
    }

    fun updateStatus(statuses: Map<ConnectivityType, ConnectivityStatus>) {
        uiHandler.post {
            val iconMapping = mapOf(
                ConnectivityType.LOCAL to localIcon,
                ConnectivityType.NETWORK to networkIcon,
                ConnectivityType.CLOUD to cloudIcon
            )
            statuses.forEach { (type, status) ->
                updateIconState(iconMapping[type] ?: return@forEach, determineIconState(status))
            }
        }
    }

    private fun determineIconState(status: ConnectivityStatus): IconState = when {
        !status.permitted -> IconState.DISABLED
        !status.available -> IconState.DISABLED
        !status.connected -> IconState.DISCONNECTED
        status.connected && status.standby -> IconState.CONNECTED_STANDBY
        else -> IconState.CONNECTED_ACTIVE
    }

    @SuppressLint("ClickableViewAccessibility")
    fun setOnDoubleTapListener(listener: () -> Unit) {
        this.doubleTapListener = listener
        gestureDetector =
            GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
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