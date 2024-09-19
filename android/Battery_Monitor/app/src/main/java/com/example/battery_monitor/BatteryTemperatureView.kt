package com.example.battery_monitor

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat

class BatteryTemperatureView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val linePaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = ContextCompat.getColor(context, R.color.graph_line_color)
    }

    private val pointPaint = Paint().apply {
        style = Paint.Style.FILL
        color = ContextCompat.getColor(context, R.color.graph_point_color)
    }

    private val textPaint = Paint().apply {
        textSize = 30f
        color = ContextCompat.getColor(context, R.color.graph_text_color)
    }

    private val path = Path()
    private var temperatureValues: List<Float> = emptyList()

    fun setTemperatureValues(values: List<Float>) {
        temperatureValues = values
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        if (temperatureValues.isEmpty()) return

        val width = width.toFloat()
        val height = height.toFloat()
        val maxTemp = temperatureValues.maxOrNull() ?: 0f
        val minTemp = temperatureValues.minOrNull() ?: 0f
        val tempRange = maxTemp - minTemp

        path.reset()
        temperatureValues.forEachIndexed { index, temp ->
            val x = width * index / (temperatureValues.size - 1)
            val y = height - (height * (temp - minTemp) / tempRange)
            if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)

            // Draw X mark
            canvas.drawLine(x - 10, y - 10, x + 10, y + 10, pointPaint)
            canvas.drawLine(x - 10, y + 10, x + 10, y - 10, pointPaint)

            // Draw temperature value
            canvas.drawText(String.format("%.1f°C", temp), x, y - 20, textPaint)
        }

        canvas.drawPath(path, linePaint)
    }
}