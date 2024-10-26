package com.example.battery_monitor

import android.app.Activity
import android.os.Handler
import android.os.Looper
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

    private val handler = Handler (Looper.getMainLooper ())

    //

    private var mqttClient: Mqtt5AsyncClient? = null
    private fun mqttConnect() {
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
                        Log.e("CloudMqtt", "MQTT broker connection failure: error=${throwable.message}")
                        disconnected()
                    } else {
                        Log.d("CloudMqtt", "MQTT broker connection success: ${connAck.reasonString.map { it.toString() }.orElse("Success")}")
                        connected()
                    }
                }
        } catch (e: Exception) {
            Log.e("CloudMqtt", "MQTT client configuration failure: error=${e.message}")
            disconnected()
        }
    }
    private fun mqttSubscribe(topic: String) : Boolean {
        val client = mqttClient ?: return false
        try {
            client.subscribeWith()
                .topicFilter(topic)
                .retainAsPublished(true)
                .noLocal(true)
                .callback { publish: Mqtt5Publish ->
                    val message = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    Log.d("CloudMqtt", "MQTT broker subscribe topic='${publish.topic}', message='$message', type=${publish.contentType.map { it.toString() }.orElse(null)}, topic=${publish.responseTopic.map { it.toString() }.orElse(null)}")
                    receive(message)
                }
                .send()
                .whenComplete { subAck: Mqtt5SubAck, throwable ->
                    if (throwable != null) {
                        Log.e("CloudMqtt", "MQTT broker subscribe topic='$topic' failure: error=${throwable.message}")
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            disconnected()
                    } else {
                        this.topic = topic
                        Log.d("CloudMqtt","MQTT broker subscribe topic='$topic' success: ${subAck.reasonCodes.joinToString(", ")}")
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e("CloudMqtt", "MQTT broker subscribe topic='$topic' failure: error=${e.message}")
            return false
        }
    }
    private fun mqttPublish(topic: String, message: String) : Boolean {
        val client = mqttClient ?: return false
        try {
            client.publishWith()
                .topic(topic)
                .payload(message.toByteArray())
                .contentType("application/json")
                .messageExpiryInterval(60)
                .send()
                .whenComplete { _, throwable ->
                    if (throwable != null) {
                        Log.e("CloudMqtt", "MQTT broker publish topic='$topic' failure: error=${throwable.message}")
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            disconnected()
                    } else
                        Log.d("CloudMqtt", "MQTT broker publish topic='$topic', message='$message'")
                }
            return true
        } catch (e: Exception) {
            Log.e("CloudMqtt", "MQTT broker publish topic='$topic' failure: error=${e.message}")
            return false
        }
    }
    private fun mqttDisconnect() {
        val client = mqttClient ?: return
        try {
            client.disconnectWith()
                .sessionExpiryInterval(0)
                .send()
                .whenComplete { _, throwable ->
                    if (throwable != null)
                        Log.e("CloudMqtt", "MQTT broker disconnect failure: error=${throwable.message}")
                    else
                        Log.d("CloudMqtt", "MQTT broker disconnect success")
                }
        } catch (e: Exception) {
            Log.e("CloudMqtt", "MQTT broker disconnect failure: error=${e.message}")
        }
        mqttClient = null
    }
    //

    private var isConnected = false
    private var isConnecting = false

    private var topic = ""

    override fun permitted() {
        statusCallback()
        if (!isConnecting && !isConnected)
            connect()
    }
    private fun connect() {
        Log.d("CloudMqtt", "Device connect initiate")
        statusCallback()
        when {
            !isEnabled() -> Log.e("CloudMqtt", "Device not enabled or available")
            !isPermitted() -> Log.e("CloudMqtt", "Device access not permitted")
            isConnecting -> Log.d("CloudMqtt", "Device connection already in progress")
            isConnected -> Log.d("CloudMqtt", "Device connection already active, will not connect")
            else -> {
                isConnecting = true
                mqttConnect()
            }
        }
    }
    private fun connected() {
        Log.d("CloudMqtt", "Device connected")
        isConnected = true
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            identify()
        }, 1000)
        if (topic.isNotEmpty())
            subscribe(topic)
    }
    private fun identify() {
        publish("${config.topic}/${connectivityInfo.deviceAddress}/peer", connectivityInfo.toJsonString())
    }
    fun subscribe(topic: String): Boolean {
        if (!isConnected) {
            Log.e("CloudMqtt", "Device subscribe '$topic' failure: not connected")
            return false
        }
        return mqttSubscribe(topic)
    }
    fun publish(topic: String, message: String): Boolean {
        if (!isConnected) {
            Log.e("CloudMqtt", "Device publish '${topic}' failure: not connected")
            return false
        }
        return mqttPublish(topic, message)
    }
    private fun receive(value: String) {
        Log.d("CloudMqtt", "Device received: $value")
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d("CloudMqtt", "Device disconnect")
        mqttDisconnect()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d("CloudMqtt", "Device disconnected")
        mqttDisconnect()
        isConnected = false
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            connect()
        }, 1000)
    }
    override fun reconnect() {
        Log.d("CloudMqtt", "Device reconnect")
        disconnect()
        handler.postDelayed({
            connect()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}