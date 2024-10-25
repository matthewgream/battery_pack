package com.example.battery_monitor

import android.app.Activity
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.Handler
import android.os.Looper
import android.util.Log
import org.java_websocket.WebSocket
import java.net.URI
import org.java_websocket.client.WebSocketClient
import org.java_websocket.framing.Framedata
import org.java_websocket.handshake.ServerHandshake

@Suppress("PropertyName")
class NetworkDeviceManagerConfig {
    val SERVICE_TYPE = "_ws._tcp."
    val SERVICE_NAME = "battery_monitor"
    val CONNECTION_TIMEOUT = 30000L // 30 seconds
}

class NetworkDeviceManager(
    activity: Activity,
    private val adapter: NetworkDeviceAdapter,
    config: NetworkDeviceManagerConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
): ConnectivityDeviceManager () {
    private val nsdManager: NsdManager = activity.getSystemService(Activity.NSD_SERVICE) as NsdManager
    private var isConnected = false
    private var isConnecting = false
    private var webSocketClient: WebSocketClient? = null

    private val scanner: NetworkDeviceScanner = NetworkDeviceScanner(
        nsdManager,
        NetworkDeviceScannerConfig(
            SERVICE_TYPE = config.SERVICE_TYPE,
            SERVICE_NAME = config.SERVICE_NAME
        ),
        connectivityInfo,
        onFound = { serviceInfo -> located (serviceInfo) }
    )

    private val checker: ConnectivityChecker = ConnectivityChecker(
        "Network",
        ConnectivityCheckerConfig(
            CONNECTION_TIMEOUT = config.CONNECTION_TIMEOUT,
            CONNECTION_CHECK = 10000L    // 10 seconds
        ),
        isConnected = { isConnected() },
        onTimeout = { reconnect() }
    )

    private fun createWebSocketClient(uri: URI): WebSocketClient {
        return object : WebSocketClient(uri) {

            private val pingHandler = Handler(Looper.getMainLooper())
            private val pingRunnable = object : Runnable {
                override fun run() {
                    if (isConnected) {
                        try {
                            sendPing()
                            pingHandler.postDelayed(this, 10000) // Send ping every 10 seconds
                        } catch (e: Exception) {
                            Log.e("Network", "Failed to send ping", e)
                            disconnected()
                        }
                    }
                }
            }

            override fun onOpen(handshakedata: ServerHandshake?) {
                Log.d("Network", "WebSocket connection established")
                connected ()
                pingHandler.post(pingRunnable)
            }
            override fun onMessage(message: String) {
                Log.d("Network", "WebSocket message received: $message")
                checker.ping()
                dataCallback(message)
            }
            override fun onClose(code: Int, reason: String, remote: Boolean) {
                Log.d("Network", "WebSocket closing: $code / $reason")
                disconnected()
            }
            override fun onError(ex: Exception?) {
                Log.e("Network", "WebSocket failure: ${ex?.message}")
                disconnected()
            }
            override fun onWebsocketPong(conn: WebSocket?, f: Framedata?) {
                Log.d("Network", "WebSocket pong received")
                checker.ping()
            }
        }
    }

    private fun identify() {
        webSocketClient?.send(connectivityInfo.toJsonString())
    }

    @Suppress("DEPRECATION")
    private fun located(serviceInfo: NsdServiceInfo) {
        val name = serviceInfo.serviceName
        val host = serviceInfo.host.hostAddress
        val port = serviceInfo.port
        Log.d("Network", "Device scan located, $name / $host:$port")
        connect("ws://$host:$port/")
    }

    private fun connect(url: String) {
        if (isConnecting || isConnected) {
            Log.d("Network", "WebSocket connection already in progress or established")
            return
        }
        isConnecting = true
        statusCallback()
        try {
            webSocketClient = createWebSocketClient(URI(url))
            webSocketClient?.connect()
        } catch (e: Exception) {
            Log.e("Network", "WebSocket connection failed", e)
            disconnected()
        }
    }

    private fun connected() {
        isConnecting = false
        isConnected = true
        statusCallback()
        checker.start()
        identify()
    }

    private fun disconnected() {
        checker.stop()
        isConnected = false
        isConnecting = false
        webSocketClient?.close()
        webSocketClient = null
        statusCallback()
    }

    override fun permitted() {
        statusCallback()
        if (!isConnected && !isConnecting)
            locate()
    }

    private fun locate() {
        Log.d("Network", "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e("Network", "Network not enabled or available")
            !isPermitted() -> Log.e("Network", "Network access not permitted")
            isConnected -> Log.d("Network", "Network connection already active, will not locate")
            else -> scanner.start()
        }
    }

    override fun disconnect() {
        scanner.stop()
        disconnected()
    }

    override fun reconnect() {
        disconnect()
        locate()
    }

    override fun isConnected(): Boolean = isConnected
}