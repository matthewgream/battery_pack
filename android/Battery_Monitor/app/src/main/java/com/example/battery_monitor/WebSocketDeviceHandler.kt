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

@Suppress("PropertyName")
class WebSocketDeviceHandlerConfig (
    val SERVICE_TYPE: String = "_ws._tcp.",
    val SERVICE_NAME: String = "BatteryMonitor",
    val CONNECTION_TIMEOUT: Long = 30000L, // 30 seconds
    val CONNECTION_CHECK: Long = 15000L // 15 seconds
)

class WebSocketDeviceHandler(
    activity: Activity,
    private val adapter: WebSocketDeviceAdapter,
    private val config: WebSocketDeviceHandlerConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
) : ConnectivityDeviceHandler() {

    private val handler = Handler(Looper.getMainLooper())

    private val scanner: WebSocketDeviceScanner = WebSocketDeviceScanner(activity, WebSocketDeviceScannerConfig(SERVICE_TYPE = config.SERVICE_TYPE, SERVICE_NAME = config.SERVICE_NAME), connectivityInfo,
        onFound = { serviceInfo -> located(serviceInfo) })
    private val checker: ConnectivityChecker = ConnectivityChecker("WebSocket", ConnectivityCheckerConfig(CONNECTION_TIMEOUT = config.CONNECTION_TIMEOUT, CONNECTION_CHECK = config.CONNECTION_CHECK),
        isConnected = { isConnected() },
        onTimeout = { reconnect() })

    //

    private var websocketClient: WebSocketClient? = null
    private fun websocketCreate (uri: URI): WebSocketClient {
        return object : WebSocketClient(uri) {
            private val pinger = object : Runnable {
                override fun run() {
                    if (isConnected) {
                        try {
                            sendPing()
                            handler.postDelayed(this, config.CONNECTION_CHECK)
                        } catch (e: Exception) {
                            Log.e("WebSocket", "sendPing failed: error=${e.message}")
                            disconnected()
                        }
                    }
                }
            }
            override fun onOpen(handshakedata: ServerHandshake?) {
                Log.d("WebSocket", "onOpen")
                connected()
                handler.post(pinger)
            }
            override fun onMessage(message: String) {
                Log.d("WebSocket", "onMessage: message=$message")
                receive(message)
            }
            override fun onClose(code: Int, reason: String, remote: Boolean) {
                Log.d("WebSocket", "onClose: remote=$remote, code=$code, reason='$reason'")
                if (remote)
                    disconnected()
            }
            override fun onError(ex: Exception?) {
                Log.e("WebSocket", "onError: error=${ex?.message}")
                disconnected()
            }
            override fun onWebsocketPong(conn: WebSocket?, f: Framedata?) {
                Log.d("WebSocket", "onWebSocketPong")
                checker.ping()
            }
        }
    }
    private fun websocketConnect (url: String): Boolean {
        try {
            websocketClient = websocketCreate(URI(url))
            websocketClient?.setConnectionLostTimeout(30)
            websocketClient?.connect()
            return true
        } catch (e: Exception) {
            Log.e("WebSocket", "Device connect failed: error=${e.message}")
            return false
        }
    }
    private fun websocketWrite (value: String) {
        try {
            websocketClient?.send(value)
        } catch (e: Exception) {
            Log.e("WebSocket", "Device send failed: error=${e.message}")
        }
    }
    private fun websocketDisconnect () {
        try {
            websocketClient?.close()
        } catch (e: Exception) {
            Log.e("WebSocket", "Device close failed: error=${e.message}")
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
        Log.d("WebSocket", "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e("WebSocket", "Device not enabled or available")
            !isPermitted() -> Log.e("WebSocket", "Device access not permitted")
            isConnecting -> Log.d("WebSocket", "Device connection already in progress")
            isConnected -> Log.d("WebSocket", "Device connection already active, will not locate")
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
        Log.d("WebSocket", "Device located '$name'/$host:$port")
        connect("ws://$host:$port/")
    }
    private fun connect(url: String) {
        Log.d("WebSocket", "Device connect to $url")
        statusCallback()
        checker.start()
        if (!websocketConnect (url))
            disconnected()
    }
    private fun connected() {
        Log.d("WebSocket", "Device connected")
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
    private fun receive(value: String) {
        Log.d("WebSocket", "Device received: $value")
        checker.ping()
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d("WebSocket", "Device disconnect")
        websocketDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d("WebSocket", "Device disconnected")
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
        Log.d("WebSocket", "Device reconnect")
        disconnect()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}