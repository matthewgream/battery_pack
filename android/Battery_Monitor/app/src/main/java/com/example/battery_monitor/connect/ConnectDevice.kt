package com.example.battery_monitor.connect

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.example.battery_monitor.utility.PermissionsManager
import com.example.battery_monitor.utility.PermissionsManagerFactory

abstract class ConnectDeviceAdapter(
    tag: String
) : ConnectComponent(tag) {
    abstract fun isEnabled(): Boolean
}

class ConnectDeviceState(
    override val tag: String,
    periodCheck: Int,
    private val periodTimeout: Int,
    private val onTimeout: () -> Unit
) : ConnectComponent(tag, periodCheck) {

    private var state = ConnectState.Disconnected
    val isConnecting: Boolean
        get() = (state == ConnectState.Connecting)
    val isConnected: Boolean
        get() = (state == ConnectState.Connected)
    val isDisconnected: Boolean
        get() = (state == ConnectState.Disconnected)

    fun connecting() {
        if (state == ConnectState.Disconnected) {
            state = ConnectState.Connecting
            start()
        } else
            Log.w(tag, "ConnectivityDeviceState::connecting while !Disconnected")
    }
    fun connected() {
        if (state == ConnectState.Connecting) {
            state = ConnectState.Connected
            ping()
        } else
            Log.w(tag, "ConnectivityDeviceState::connected while !Connecting")
    }
    fun disconnected() {
        if (state != ConnectState.Disconnected) {
            state = ConnectState.Disconnected
            stop()
        }
    }

    private var checked: Long = 0
    override fun onStart() {
        checked = System.currentTimeMillis()
    }
    override fun onTimer(): Boolean {
        if (state != ConnectState.Disconnected && (System.currentTimeMillis() - checked) > (periodTimeout*1000L)) {
            Log.d(tag, "Connection state timeout")
            onTimeout()
            return false
        }
        return true
    }
    fun ping() {
        checked = System.currentTimeMillis() // even if not active
    }
}

abstract class ConnectDeviceHandler(
    val tag: String,
    private val statusCallback: () -> Unit,
    activeCheck: Int,
    activeTimeout: Int,
    private val isPermitted: () -> Boolean,
    private val isAvailable: () -> Boolean
) {
    private val handler = Handler(Looper.getMainLooper())

    abstract fun doConnectionStart(): Boolean
    abstract fun doConnectionIdentify(): Boolean
    abstract fun doConnectionStop()

    fun permitted() {
        statusCallback()
        if (state.isDisconnected)
            initiate()
    }
    private fun initiate() {
        when {
            !isAvailable() -> Log.e(tag, "Device not enabled or available")
            !isPermitted() -> Log.e(tag, "Device access not permitted")
            state.isConnecting -> Log.d(tag, "Device connection already in progress")
            state.isConnected -> Log.d(tag, "Device connection already active, will not initiate")
            else -> {
                state.connecting()
                statusCallback()
                if (!doConnectionStart())
                    handler.postDelayed({
                        initiate()
                    }, 1000)
                return
            }
        }
        statusCallback()
    }
    fun setConnectionIsConnected() {
        state.connected()
        statusCallback()
        handler.postDelayed({
            if (!doConnectionIdentify())
                setConnectionDoReconnect()
        }, 1000)
    }
    fun setConnectionIsActive() {
        state.ping()
    }
    fun setConnectionIsDisconnected() {
        doConnectionStop()
        state.disconnected()
        statusCallback()
    }
    fun setConnectionDoReconnect() {
        Log.d(tag, "Device reconnect")
        doConnectionStop()
        state.disconnected()
        statusCallback()
        handler.postDelayed({
            initiate()
        }, 1000)
    }

    private val state = ConnectDeviceState(tag, activeCheck, activeTimeout,
        onTimeout = { setConnectionDoReconnect() })
    fun isConnected(): Boolean = state.isConnected
}

@Suppress("EmptyMethod", "PublicApiImplicitType")
abstract class ConnectDeviceManager<TAdapter : ConnectDeviceAdapter, TDevice : ConnectDeviceHandler>(
    tag: String,
    protected val activity: Activity,
    permissionsRequired: Array<String>,
    protected val connectInfo: ConnectInfo,
    protected val dataCallback: (String) -> Unit,
    protected val statusCallback: () -> Unit
) {
    private val handler = Handler(Looper.getMainLooper())

    protected val permissions: PermissionsManager = PermissionsManagerFactory(activity).create(tag, permissionsRequired)
    protected abstract val adapter: TAdapter
    protected abstract val device: TDevice

    fun onCreate() {
        permissions.requestPermissions(
            onPermissionsAllowed = {
                handler.postDelayed({
                    onPermitted()
                }, 50)
            }
        )
    }
    fun onDestroy() {
        adapter.stop()
        onDisconnected()
    }
    fun onPause() {}
    fun onResume() {}
    fun onPowerSave() {}
    fun onPowerBack() {}

    fun onDoubleTap() = device.setConnectionDoReconnect()
    private fun onPermitted() {
        adapter.start()
        device.permitted()
    }
    private fun onDisconnected() = device.setConnectionIsDisconnected()
    protected fun onEnabled() = device.permitted()
    protected fun onDisabled() = device.setConnectionIsDisconnected()

    fun isAvailable(): Boolean = adapter.isEnabled()
    fun isPermitted(): Boolean = permissions.allowed
    fun isConnected(): Boolean = device.isConnected()
}