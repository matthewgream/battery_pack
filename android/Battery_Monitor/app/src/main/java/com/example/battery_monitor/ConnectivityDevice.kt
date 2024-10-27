package com.example.battery_monitor

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log

abstract class ConnectivityDeviceAdapter(
    tag: String
) : ConnectivityComponent(tag) {
    abstract fun isEnabled(): Boolean
}

class ConnectivityDeviceState(
    override val tag: String,
    periodCheck: Int,
    private val periodTimeout: Int,
    private val onTimeout: () -> Unit
) : ConnectivityComponent(tag, periodCheck) {

    var isConnecting: Boolean = false
        private set
    var isConnected: Boolean = false
        private set

    fun connecting() {
        if (!isConnecting && !isConnected) {
            isConnecting = true
            isConnected = false
            start()
        } else
            Log.w (tag, "connecting: in bad state")
    }
    fun connected() {
        if (isConnecting && !isConnected) {
            isConnecting = false
            isConnected = true
            ping()
        } else
            Log.w (tag, "connected: in bad state")
    }
    fun disconnected() {
        isConnecting = false
        isConnected = false
        stop()
    }

    private var checked: Long = 0
    override fun onStart() {
        checked = System.currentTimeMillis()
    }
    override fun onTimer(): Boolean {
        if ((isConnecting || isConnected) && System.currentTimeMillis() - checked > (periodTimeout*1000L)) {
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

abstract class ConnectivityDeviceHandler(
    val tag: String,
    private val statusCallback: () -> Unit,
    activeCheck: Int,
    activeTimeout: Int,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
) {
    private val handler = Handler(Looper.getMainLooper())

    abstract fun doConnectionStart() : Boolean
    abstract fun doConnectionIdentify() : Boolean
    abstract fun doConnectionStop()

    fun permitted() {
        statusCallback()
        if (!state.isConnecting && !state.isConnected)
            initiate()
    }
    private fun initiate() {
        when {
            !isEnabled() -> Log.e(tag, "Device not enabled or available")
            !isPermitted() -> Log.e(tag, "Device access not permitted")
            state.isConnecting -> Log.d(tag, "Device connection already in progress")
            state.isConnected -> Log.d(tag, "Device connection already active, will not locate")
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

    private val state = ConnectivityDeviceState(tag, activeCheck, activeTimeout,
        onTimeout = { setConnectionDoReconnect() })
    fun isConnected (): Boolean = state.isConnected
}

@Suppress("EmptyMethod")
abstract class ConnectivityDeviceManager<TAdapter : ConnectivityDeviceAdapter, TDevice : ConnectivityDeviceHandler, TConfig>(
    tag: String,
    protected val activity: Activity,
    permissionsRequired: Array<String>,
    protected val connectivityInfo: ConnectivityInfo,
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

    @Suppress("PublicApiImplicitType")
    protected fun onDisconnected() = device.setConnectionIsDisconnected()
    @Suppress("PublicApiImplicitType")
    fun onDoubleTap() = device.setConnectionDoReconnect()
    protected fun onPermitted() {
        adapter.start()
        device.permitted()
    }

    fun isAvailable(): Boolean = adapter.isEnabled()
    fun isPermitted(): Boolean = permissions.allowed
    fun isConnected(): Boolean = device.isConnected()
}