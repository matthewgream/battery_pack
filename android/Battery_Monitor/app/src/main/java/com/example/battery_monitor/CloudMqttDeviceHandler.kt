package com.example.battery_monitor

import android.app.Activity
import android.util.Log
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt5.Mqtt5AsyncClient
import com.hivemq.client.mqtt.mqtt5.message.connect.connack.Mqtt5ConnAck
import com.hivemq.client.mqtt.mqtt5.message.publish.Mqtt5Publish
import com.hivemq.client.mqtt.mqtt5.message.subscribe.suback.Mqtt5SubAck
import java.nio.charset.StandardCharsets

class CloudMqttDeviceHandler(
    tag: String,
    @Suppress("unused") private val activity: Activity,
    @Suppress("unused") private val adapter: AdapterNetworkInternet,
    private val config: CloudMqttDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isAvailable: () -> Boolean,
    isPermitted: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, 0, 0, isAvailable, isPermitted) {

    //

    private var mqttClient: Mqtt5AsyncClient? = null
    private fun mqttConnect(): Boolean {
        try {
            mqttClient = MqttClient.builder()
                .useMqttVersion5()
                .identifier(connectivityInfo.identity)
                .serverHost(config.host)
                .serverPort(config.port)
                .automaticReconnectWithDefaultConfig()
                .addConnectedListener { _ ->
                    Log.d(tag, "MQTT broker Keep-alive received")
                    onMqttKeepalive()
                }
                .build()
                .toAsync()
            val connect = mqttClient?.connectWith()
                ?.cleanStart(true)
                ?.sessionExpiryInterval(config.connectionActiveTimeout.toLong())
                ?.keepAlive(config.connectionActiveCheck)
            if (config.user != null && config.pass != null)
                connect?.simpleAuth()
                    ?.username(config.user)
                    ?.password(config.pass.toByteArray())
                    ?.applySimpleAuth()
            connect?.send()
                ?.whenComplete { connAck: Mqtt5ConnAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker connection failure: exception", throwable)
                        onMqttError()
                    } else {
                        Log.d(tag, "MQTT broker connection success: ${connAck.reasonString.map { it.toString() }.orElse("Success")}")
                        onMqttConnected()
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT client configuration failure: exception", e)
        }
        return false
    }
    private fun mqttSubscribe(topic: String): Boolean {
        val client = mqttClient ?: return false
        try {
            client.subscribeWith()
                .topicFilter(topic)
                .retainAsPublished(true)
                .noLocal(true)
                .callback { publish: Mqtt5Publish ->
                    val message = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    Log.d(tag, "MQTT broker subscribe topic='${publish.topic}', message='$message'")
                    onMqttReceived(message)
                }
                .send()
                .whenComplete { subAck: Mqtt5SubAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker subscribe topic='$topic' failure: error=${throwable.message}")
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            onMqttDisconnected()
                        else
                            onMqttError()
                    } else {
                        this.topic = topic
                        Log.d(tag,"MQTT broker subscribe topic='$topic' success: ${subAck.reasonCodes.joinToString(", ")}")
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker subscribe topic='$topic' failure: exception", e)
        }
        return false
    }
    private fun mqttUnsubscribe(topic: String): Boolean {
        val client = mqttClient ?: return false
        try {
            client.unsubscribeWith()
                .topicFilter(topic)
                .send()
                .whenComplete { unsubAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure: exception", throwable)
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            onMqttDisconnected()
                        else
                            onMqttError()
                    } else {
                        this.topic = ""
                        Log.d(tag, "MQTT broker unsubscribe topic='$topic' success: ${unsubAck.reasonCodes.joinToString(", ")}")
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure: exception", e)
        }
        return false
    }
    private fun mqttPublish(topic: String, message: String): Boolean {
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
                        Log.e(tag, "MQTT broker publish topic='$topic' failure: exception", throwable)
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            onMqttDisconnected()
                        else
                            onMqttError()
                    } else
                        Log.d(tag, "MQTT broker publish topic='$topic', message='$message'")
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker publish topic='$topic' failure: exception", e)
        }
        return false
    }
    private fun mqttDisconnect() {
        val client = mqttClient ?: return
        try {
            client.disconnectWith()
                .sessionExpiryInterval(0)
                .send()
                .whenComplete { _, throwable ->
                    if (throwable != null)
                        Log.e(tag, "MQTT broker disconnect failure: exception", throwable)
                    else
                        Log.d(tag, "MQTT broker disconnect success")
                }
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker disconnect failure: exception", e)
        }
        mqttClient = null
    }

    //

    private var root = "${config.root}/${connectivityInfo.deviceAddress}"
    private var topic = ""

    override fun doConnectionStart(): Boolean  {
        Log.d(tag, "Device connect")
        return mqttConnect()
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        mqttDisconnect()
    }
    override fun doConnectionIdentify(): Boolean {
        return mqttPublish("${root}/peer", connectivityInfo.toJsonString())
    }

    //

    private fun onMqttConnected() {
        Log.d(tag, "Device connected")
        setConnectionIsConnected()
        if (topic.isNotEmpty())
            if (!mqttSubscribe(topic))
                setConnectionDoReconnect()
    }
    private fun onMqttDisconnected() {
        Log.d(tag, "Device disconnected")
        setConnectionDoReconnect()
    }
    private fun onMqttReceived(value: String) {
        Log.d(tag, "Device received: $value")
        setConnectionIsActive()
        dataCallback(value)
    }
    private fun onMqttError() {
        Log.d(tag, "Device error")
        setConnectionDoReconnect()
    }
    private fun onMqttKeepalive() {
        setConnectionIsActive()
    }

    //

    fun subscribe(): Boolean {
        if (topic.isNotEmpty()) {
            Log.e(tag, "Device subscribe: failure, already subscribed")
            return false
        }
        topic = "${root}/#"
        if (!isConnected()) {
            Log.d(tag, "Device subscribe: pending, upon connection")
            return true
        }
        Log.d(tag, "Device subscribe")
        return mqttSubscribe(topic)
    }
    fun unsubscribe(): Boolean {
        if (topic.isEmpty()) {
            Log.e(tag, "Device unsubscribe: failure, not subscribed")
            return false
        }
        val topicOld = topic
        topic = ""
        if (!isConnected()) {
            Log.d(tag, "Device unsubscribe: cleared, not connected")
            return true
        }
        Log.d(tag, "Device unsubscribe")
        return mqttUnsubscribe(topicOld)
    }
    fun publish(type: String, message: String): Boolean {
        if (!isConnected()) {
            Log.e(tag, "Device publish failure: not connected")
            return false
        }
        Log.d(tag, "Device publish")
        return mqttPublish("${root}/${type}", message)
    }
}