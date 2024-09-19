package com.example.battery_monitor

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

fun formatTime (timestamp: Long): String {
    return SimpleDateFormat ("HH:mm:ss", Locale.getDefault ()).format (Date (timestamp * 1000))
}