package com.example.battery_monitor.connect

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
import androidx.annotation.ColorRes
import androidx.annotation.IdRes
import androidx.annotation.LayoutRes

class ConnectStatusView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : LinearLayout(context, attrs, defStyleAttr) {

    data class Config(
        @LayoutRes val layout: Int,
        val views: Views,
        val colors: Colors,
        val elevation: Float = 4f
    ) {
        data class Views(
            @IdRes val direct: Int,
            @IdRes val local: Int,
            @IdRes val cloud: Int
        )
        data class Colors(
            @ColorRes val disabled: Int,
            @ColorRes val disconnected: Int,
            @ColorRes val connectedStandby: Int,
            @ColorRes val connectedActive: Int
        )
    }

    private val uiHandler = Handler(Looper.getMainLooper())
    private lateinit var doubleTapListener: () -> Unit
    private lateinit var gestureDetector: GestureDetector

    private lateinit var config: Config

    private lateinit var iconDirect: ImageView
    private lateinit var iconLocal: ImageView
    private lateinit var iconCloud: ImageView

    private enum class IconState {
        DISABLED, // Grey - No permissions or not available
        DISCONNECTED, // Red - Available but not connected
        CONNECTED_STANDBY, // Orange - Connected but inactive
        CONNECTED_ACTIVE // Green - Connected and active
    }

    private enum class ConnectivityPriority(val type: ConnectType) {
        DIRECT(ConnectType.DIRECT),
        LOCAL(ConnectType.LOCAL),
        CLOUD(ConnectType.CLOUD);

        companion object {
            fun fromType(type: ConnectType): ConnectivityPriority {
                return values().first { it.type == type }
            }
        }
    }

    init {
        orientation = VERTICAL
    }

    fun initialize(config: Config) {
        LayoutInflater.from(context).inflate(config.layout, this, true)

        this.config = config
        iconDirect = findViewById(config.views.direct)
        iconLocal = findViewById(config.views.local)
        iconCloud = findViewById(config.views.cloud)

        elevation = config.elevation
    }

    private fun updateIconState(icon: ImageView, state: IconState) {
        val colorResId = when (state) {
            IconState.DISABLED -> config.colors.disabled
            IconState.DISCONNECTED -> config.colors.disconnected
            IconState.CONNECTED_STANDBY -> config.colors.connectedStandby
            IconState.CONNECTED_ACTIVE -> config.colors.connectedActive
        }
        icon.setColorFilter(context.getColor(colorResId))
    }

    fun updateStatus(statuses: Map<ConnectType, ConnectStatus>) {
        uiHandler.post {
            // Find the highest priority connected device
            val highestPriorityConnected = ConnectivityPriority.values()
                .firstOrNull { priority ->
                    statuses[priority.type]?.let { status ->
                        status.permitted && status.available && status.connected
                    } ?: false
                }

            val iconMapping = mapOf(
                ConnectType.DIRECT to iconDirect,
                ConnectType.LOCAL to iconLocal,
                ConnectType.CLOUD to iconCloud
            )

            statuses.forEach { (type, status) ->
                val icon = iconMapping[type] ?: return@forEach
                val priority = ConnectivityPriority.fromType(type)

                val iconState = when {
                    !status.permitted || !status.available -> IconState.DISABLED
                    !status.connected -> IconState.DISCONNECTED
                    highestPriorityConnected != null && priority != highestPriorityConnected ->
                        IconState.CONNECTED_STANDBY
                    else -> IconState.CONNECTED_ACTIVE
                }

                updateIconState(icon, iconState)
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    fun setOnDoubleTapListener(listener: () -> Unit) {
        this.doubleTapListener = listener
        gestureDetector = GestureDetector(context,
            object : GestureDetector.SimpleOnGestureListener() {
                override fun onDoubleTap(e: MotionEvent): Boolean {
                    doubleTapListener.invoke()
                    return true
                }
            }
        )

        setOnTouchListener { _, event ->
            gestureDetector.onTouchEvent(event)
            true
        }
    }
}