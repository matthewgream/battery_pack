package com.example.battery_monitor

import android.app.Activity
import android.net.nsd.NsdServiceInfo
import android.os.Handler
import android.os.Looper
import android.util.Log
import org.java_websocket.WebSocket
import org.java_websocket.client.WebSocketClient
import org.java_websocket.framing.Framedata
import org.java_websocket.handshake.ServerHandshake
import java.net.URI

class WebSocketDeviceHandler(
    private val tag: String,
    activity: Activity,
    @Suppress("unused") private val adapter: AdapterWifi,
    private val config: WebSocketDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
) : ConnectivityDeviceHandler() {

    private val handler = Handler(Looper.getMainLooper())

    private val scanner: WebSocketDeviceScanner = WebSocketDeviceScanner(tag, activity,
        WebSocketDeviceScanner.Config (config.serviceType, config.serviceName, config.connectionScanDelay, config.connectionScanPeriod), connectivityInfo,
        onFound = { serviceInfo -> located(serviceInfo) })
    private val checker: ConnectivityChecker = ConnectivityChecker(tag, config.connectionActiveCheck, config.connectionActiveTimeout,
        isConnected = { isConnected() },
        onTimeout = { reconnect() })

    //

    private var websocketClient: WebSocketClient? = null
    private fun websocketCreate (uri: URI): WebSocketClient {
        return object : WebSocketClient(uri) {
            override fun onOpen(handshakedata: ServerHandshake?) {
                Log.d(tag, "onOpen")
                connected()
            }
            override fun onMessage(message: String) {
                Log.d(tag, "onMessage: message=$message")
                receive(message)
            }
            override fun onClose(code: Int, reason: String, remote: Boolean) {
                Log.d(tag, "onClose: remote=$remote, code=$code, reason='$reason'")
                if (remote)
                    disconnected()
            }
            override fun onError(ex: Exception?) {
                Log.e(tag, "onError: error=${ex?.message}")
                disconnected()
            }
            override fun onWebsocketPong(conn: WebSocket?, f: Framedata?) {
                Log.d(tag, "onWebSocketPong")
                keepalive()
            }
        }
    }
    private fun websocketConnect (url: String): Boolean {
        try {
            websocketClient = websocketCreate(URI(url))
            @Suppress("UsePropertyAccessSyntax")
            websocketClient?.setConnectionLostTimeout(config.connectionActiveCheck)
            websocketClient?.connect()
            return true
        } catch (e: Exception) {
            Log.e(tag, "Device connect failed: exception", e)
            return false
        }
    }
    private fun websocketWrite (value: String) {
        try {
            websocketClient?.send(value)
        } catch (e: Exception) {
            Log.e(tag, "Device send failed: exception", e)
        }
    }
    private fun websocketDisconnect () {
        try {
            websocketClient?.close()
        } catch (e: Exception) {
            Log.e(tag, "Device close failed: exception", e)
        }
        websocketClient = null
    }

    //

    private var isConnecting = false
    private var isConnected = false

    override fun permitted() {
        statusCallback()
        if (!isConnecting && !isConnected)
            locate()
    }
    private fun locate() {
        Log.d(tag, "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e(tag, "Device not enabled or available")
            !isPermitted() -> Log.e(tag, "Device access not permitted")
            isConnecting -> Log.d(tag, "Device connection already in progress")
            isConnected -> Log.d(tag, "Device connection already active, will not locate")
            else -> {
                isConnecting = true
                scanner.start()
            }
        }
    }
    @Suppress("DEPRECATION")
    private fun located(serviceInfo: NsdServiceInfo) {
        val name = serviceInfo.serviceName
        val host = serviceInfo.host.hostAddress
        val port = serviceInfo.port
        Log.d(tag, "Device located '$name'/$host:$port")
        connect("ws://$host:$port/")
    }
    private fun connect(url: String) {
        Log.d(tag, "Device connect to $url")
        statusCallback()
        checker.start()
        if (!websocketConnect (url))
            disconnected()
    }
    private fun connected() {
        Log.d(tag, "Device connected")
        checker.ping()
        isConnecting = false
        isConnected = true
        statusCallback()
        handler.postDelayed({
            identify()
        }, 1000)
    }
    private fun identify() {
        websocketWrite(connectivityInfo.toJsonString())
    }
    private fun keepalive() {
        checker.ping()
    }
    private fun receive(value: String) {
        Log.d(tag, "Device received: $value")
        checker.ping()
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d(tag, "Device disconnect")
        websocketDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d(tag, "Device disconnected")
        websocketDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun reconnect() {
        Log.d(tag, "Device reconnect")
        disconnect()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}