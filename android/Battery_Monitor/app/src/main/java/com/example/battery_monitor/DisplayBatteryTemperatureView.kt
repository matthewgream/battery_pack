package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat

class DisplayBatteryTemperatureView @JvmOverloads constructor (
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View (context, attrs, defStyleAttr) {

    private val paintLine = Paint ().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = ContextCompat.getColor (context, R.color.graph_line_color)
    }
    private val paintPoint = Paint ().apply {
        style = Paint.Style.FILL
        color = ContextCompat.getColor (context, R.color.graph_point_color)
    }
    private val paintText = Paint ().apply {
        textSize = 30f
        color = ContextCompat.getColor (context, R.color.graph_text_color)
    }

    private val path = Path ()
    private var temperatureValues: List<Float> = emptyList ()

    fun setTemperatureValues (values: List<Float>) {
        temperatureValues = values
        invalidate ()
    }

    @SuppressLint("DefaultLocale")
    override fun onDraw (canvas: Canvas) {
        super.onDraw (canvas)

        if (temperatureValues.isEmpty ()) return

        val width = width.toFloat ()
        val height = height.toFloat ()
        val count = temperatureValues.size
        val maxTemp = temperatureValues.maxOrNull () ?: 0f
        val minTemp = temperatureValues.minOrNull () ?: 0f

        path.reset ()
        temperatureValues.forEachIndexed { index, temp ->
            val x = width * index / (count - 1)
            val y = height - (height * (temp - minTemp) / (maxTemp - minTemp))
            if (index == 0) path.moveTo (x, y) else path.lineTo (x, y)
            canvas.drawLine (x - 10, y - 10, x + 10, y + 10, paintPoint)
            canvas.drawLine (x - 10, y + 10, x + 10, y - 10, paintPoint)
            canvas.drawText (String.format ("%.1f°C", temp), x, y - 20, paintText)
        }
        canvas.drawPath (path, paintLine)
    }
}