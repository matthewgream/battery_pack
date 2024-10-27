package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.LayoutInflater
import android.view.MotionEvent
import android.widget.ImageView
import android.widget.LinearLayout
import android.graphics.drawable.LayerDrawable

class DataViewConnectivityStatus @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : LinearLayout(context, attrs, defStyleAttr) {

    private val uiHandler = Handler(Looper.getMainLooper())
    private lateinit var doubleTapListener: () -> Unit
    private lateinit var gestureDetector: GestureDetector

    private val iconDirect: ImageView
    private val iconLocal: ImageView
    private val iconCloud: ImageView

    private enum class IconState {
        DISABLED, // Grey - No permissions or not available
        DISCONNECTED, // Red - Available but not connected
        CONNECTED_STANDBY, // Orange - Connected but inactive
        CONNECTED_ACTIVE // Green - Connected and active
    }

    // Define connection priority order
    private enum class ConnectivityPriority(val type: ConnectivityType) {
        DIRECT(ConnectivityType.DIRECT),
        LOCAL(ConnectivityType.LOCAL),
        CLOUD(ConnectivityType.CLOUD);

        companion object {
            fun fromType(type: ConnectivityType): ConnectivityPriority {
                return values().first { it.type == type }
            }
        }
    }

    init {
        orientation = VERTICAL
        LayoutInflater.from(context).inflate(R.layout.view_connectivity_status, this, true)

        iconDirect = findViewById(R.id.iconDirect)
        iconLocal = findViewById(R.id.iconLocal)
        iconCloud = findViewById(R.id.iconCloud)

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
            // Find the highest priority connected device
            val highestPriorityConnected = ConnectivityPriority.values()
                .firstOrNull { priority ->
                    statuses[priority.type]?.let { status ->
                        status.permitted && status.available && status.connected
                    } ?: false
                }

            val iconMapping = mapOf(
                ConnectivityType.DIRECT to iconDirect,
                ConnectivityType.LOCAL to iconLocal,
                ConnectivityType.CLOUD to iconCloud
            )

            statuses.forEach { (type, status) ->
                val icon = iconMapping[type] ?: return@forEach
                val priority = ConnectivityPriority.fromType(type)

                val iconState = when {
                    !status.permitted || !status.available -> IconState.DISABLED
                    !status.connected -> IconState.DISCONNECTED
                    highestPriorityConnected != null && priority != highestPriorityConnected -> IconState.CONNECTED_STANDBY
                    else -> IconState.CONNECTED_ACTIVE
                }

                updateIconState(icon, iconState)
            }
        }
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