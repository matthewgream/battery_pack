package com.example.battery_monitor

import android.app.Activity
import android.os.Handler
import android.os.Looper

abstract class ConnectivityDeviceAdapter (
    tag: String
): ConnectivityComponent(tag) {
    abstract fun isEnabled(): Boolean
}
abstract class ConnectivityDeviceHandler {
    abstract fun isConnected(): Boolean
    abstract fun disconnected()
    abstract fun reconnect()
    abstract fun permitted()
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

    protected fun onDisconnected() = device.disconnected()
    fun onDoubleTap() = device.reconnect()
    protected fun onPermitted() {
        adapter.start()
        device.permitted()
    }

    fun isAvailable(): Boolean = adapter.isEnabled()
    fun isPermitted(): Boolean = permissions.allowed
    fun isConnected(): Boolean = device.isConnected()
}