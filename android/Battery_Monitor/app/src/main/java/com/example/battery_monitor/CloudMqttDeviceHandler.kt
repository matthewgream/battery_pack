package com.example.battery_monitor

import android.app.Activity
import android.util.Log
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt5.Mqtt5AsyncClient
import com.hivemq.client.mqtt.mqtt5.message.connect.connack.Mqtt5ConnAck
import com.hivemq.client.mqtt.mqtt5.message.publish.Mqtt5Publish
import com.hivemq.client.mqtt.mqtt5.message.subscribe.suback.Mqtt5SubAck
import java.nio.charset.StandardCharsets
import java.util.UUID

class CloudMqttDeviceHandlerConfig(
    val host: String,
    val port: Int = 1883,
    val user: String? = null,
    val pass: String? = null,
    val client: String = UUID.randomUUID().toString(),
    val topic: String
)

class CloudMqttDeviceHandler(
    private val activity: Activity,
    private val adapter: CloudMqttDeviceAdapter,
    private val config: CloudMqttDeviceHandlerConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isEnabled: () -> Boolean,
    private val isPermitted: () -> Boolean
) : ConnectivityDeviceHandler() {
    //    private val handler = Handler (Looper.getMainLooper ())
    private var mqttClient: Mqtt5AsyncClient? = null
    private var isConnected = false
    private var isConnecting = false
    private var topic = ""

    override fun permitted() {
        statusCallback()
        if (!isConnected && !isConnecting)
            connect()
    }

    private fun connect() {
        Log.d("Cloud", "Cloud connect initiate")
        statusCallback()
        when {
            !isEnabled() -> Log.e("Cloud", "Network not enabled or available")
            !isPermitted() -> Log.e("Cloud", "Cloud access not permitted")
            isConnecting -> Log.d("Cloud", "Cloud connection already in progress")
            isConnected -> Log.d("Cloud", "Cloud connection already active, will not connect")
            else -> {
                isConnecting = true
                connectMqtt()
            }
        }
    }

    private fun connectMqtt() {
        try {
            mqttClient = MqttClient.builder()
                .useMqttVersion5()
                .identifier(config.client)
                .serverHost(config.host)
                .serverPort(config.port)
                .automaticReconnectWithDefaultConfig()
                .build()
                .toAsync()

            val connect = mqttClient?.connectWith()
                ?.cleanStart(true)
                ?.sessionExpiryInterval(30L)
                ?.keepAlive(30)

            if (config.user != null && config.pass != null)
                connect?.simpleAuth()
                    ?.username(config.user)
                    ?.password(config.pass.toByteArray())
                    ?.applySimpleAuth()

            connect?.send()
                ?.whenComplete { connAck: Mqtt5ConnAck, throwable ->
                    if (throwable != null) {
                        Log.e("Cloud", "MQTT broker connection failure: ", throwable)
                        isConnected = false
                        isConnecting = false
                        statusCallback()
                        // XXX retry
                    } else {
                        Log.d(
                            "Cloud",
                            "MQTT broker connection success: ${
                                connAck.reasonString.map { it.toString() }.orElse("Success")
                            }"
                        )
                        isConnected = true
                        isConnecting = false
                        statusCallback()
                        identify()
                        if (topic.isNotEmpty())
                            subscribe(topic)
                    }
                }
        } catch (e: Exception) {
            Log.e("Cloud", "MQTT client configuration failure: ", e)
            isConnected = false
            isConnecting = false
            statusCallback()
            // XXX retry
        }
    }

    fun subscribe(topic: String) {
        if (!isConnected) {
            Log.e("Cloud", "MQTT broker subscribe '$topic' failure: not connected")
            return
        }
        try {
            mqttClient?.subscribeWith()
                ?.topicFilter(topic)
                ?.retainAsPublished(true)
                ?.noLocal(true)
                ?.callback { publish: Mqtt5Publish ->
                    val message = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    Log.d(
                        "Cloud",
                        "MQTT broker subscribe '${publish.topic}' message: '$message' [type=${
                            publish.contentType.map { it.toString() }.orElse(null)
                        }, topic=${publish.responseTopic.map { it.toString() }.orElse(null)}]"
                    )
                    dataCallback(message)
                }
                ?.send()
                ?.whenComplete { subAck: Mqtt5SubAck, throwable ->
                    if (throwable != null)
                        Log.e("Cloud", "MQTT broker subscribe '$topic' failure: ", throwable)
                    else {
                        this.topic = topic
                        Log.d(
                            "Cloud",
                            "MQTT broker subscribe '$topic' success: ${
                                subAck.reasonCodes.joinToString(", ")
                            }"
                        )
                    }
                }
        } catch (e: Exception) {
            Log.e("Cloud", "MQTT broker subscribe '$topic' failure: ", e)
        }
    }

    fun publish(topic: String, message: String) {
        if (!isConnected) {
            Log.e("Cloud", "MQTT broker publish '${topic}' failure: not connected")
            return
        }
        try {
            mqttClient?.publishWith()
                ?.topic(topic)
                ?.payload(message.toByteArray())
                ?.contentType("application/json")
                ?.messageExpiryInterval(60)
                ?.send()
                ?.whenComplete { _, throwable ->
                    if (throwable != null)
                        Log.e("Cloud", "MQTT broker publish '${topic}' failure: ", throwable)
                    else
                        Log.d("Cloud", "MQTT broker publish '${topic}' success")
                }
        } catch (e: Exception) {
            Log.e("Cloud", "MQTT broker publish '${topic}' failure: ", e)
        }
    }

    override fun disconnect() {
        try {
            mqttClient?.disconnectWith()
                ?.sessionExpiryInterval(0)
                ?.send()
                ?.whenComplete { _, throwable ->
                    if (throwable != null)
                        Log.e("Cloud", "MQTT broker disconnect failure: ", throwable)
                    else
                        Log.d("Cloud", "MQTT broker disconnect success")
                    mqttClient = null
                    isConnected = false
                    isConnecting = false
                    statusCallback()
                }
        } catch (e: Exception) {
            Log.e("Cloud", "MQTT broker disconnect failure: ", e)
            mqttClient = null
            isConnected = false
            isConnecting = false
            statusCallback()
        }
    }

    private fun identify() {
        publish("${config.topic}/${connectivityInfo.deviceAddress}/peer", connectivityInfo.toJsonString())
    }

    override fun reconnect() {
//        disconnect ()
//        connect ()
    }

    override fun isConnected(): Boolean = isConnected
}