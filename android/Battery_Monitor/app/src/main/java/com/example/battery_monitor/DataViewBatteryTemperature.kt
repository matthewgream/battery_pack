package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.DashPathEffect
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View

class DataViewBatteryTemperature @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val paintLine = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = context.getColor (R.color.graph_line_color)
    }
    private val paintPoint = Paint().apply {
        style = Paint.Style.FILL
        color = context.getColor(R.color.graph_point_color)
    }
    private val paintText = Paint().apply {
        textSize = 30f
        color = context.getColor(R.color.graph_text_color)
    }
    private val paintDashedLine = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
        color = context.getColor(R.color.graph_line_color)
        pathEffect = DashPathEffect(floatArrayOf(10f, 10f), 0f)
    }
    private val paintThresholdLine = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
        color = context.getColor(android.R.color.holo_red_light)
    }

    private var temperatureValues: List<Float> = emptyList()
    private var historicalTemperatures: MutableList<List<Float>> = mutableListOf()
    private val maxHistorySize = 16

    fun setTemperatureValues(values: List<Float>) {
        historicalTemperatures.add(0, values)
        if (historicalTemperatures.size > maxHistorySize) {
            historicalTemperatures.removeAt(maxHistorySize)
        }
        temperatureValues = values
        invalidate()
    }
    @SuppressLint("DefaultLocale", "DrawAllocation")
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        if (temperatureValues.isEmpty()) return

        val width = width.toFloat()
        val height = height.toFloat()
        val count = temperatureValues.size

        val horizontalPadding = width / (count * 2)
        val drawableWidth = width - (2 * horizontalPadding)

        val maxTemp = historicalTemperatures.flatten ().maxOrNull () ?: 0f
        val minTemp = historicalTemperatures.flatten ().minOrNull () ?: 0f
        val thresholds = listOf (
            Triple (25f, "25°C", context.getColor (R.color.threshold_min)),
            Triple (35f, "35°C", context.getColor (R.color.threshold_warning)),
            Triple (45f, "45°C", context.getColor (R.color.threshold_max))
        )

        val topPadding = 50f
        val bottomPadding = 20f
        val graphHeight = height - topPadding - bottomPadding

        for (i in historicalTemperatures.size - 1 downTo 1) {
            val olderValues = historicalTemperatures [i]
            val olderFraction = i.toFloat() / (historicalTemperatures.size - 1)
            val newerValues = historicalTemperatures [i - 1]
            val newerFraction = (i - 1).toFloat() / (historicalTemperatures.size - 1)
            val areaPath = Path ()
            olderValues.forEachIndexed { index, temp ->
                val x = horizontalPadding + (drawableWidth * index / (count - 1))
                val y = topPadding + graphHeight - (graphHeight * (temp - minTemp) / (maxTemp - minTemp))
                if (index == 0) areaPath.moveTo(x, y) else areaPath.lineTo(x, y)
            }
            for (index in newerValues.indices.reversed ()) {
                val x = horizontalPadding + (drawableWidth * index / (count - 1))
                val y = topPadding + graphHeight - (graphHeight * (newerValues [index] - minTemp) / (maxTemp - minTemp))
                areaPath.lineTo (x, y)
            }
            areaPath.close()
            val areaPaint = Paint ().apply {
                style = Paint.Style.FILL
                shader = LinearGradient (0f, 0f, 0f, height, interpolateColor (olderFraction), interpolateColor (newerFraction), Shader.TileMode.CLAMP)
            }
            canvas.drawPath(areaPath, areaPaint)
        }

        for (i in historicalTemperatures.size - 1 downTo 1) {
            val values = historicalTemperatures [i]
            val fraction = i.toFloat() / (historicalTemperatures.size - 1)
            val linePath = Path ()
            values.forEachIndexed { index, temp ->
                val x = horizontalPadding + (drawableWidth * index / (count - 1))
                val y = topPadding + graphHeight - (graphHeight * (temp - minTemp) / (maxTemp - minTemp))
                if (index == 0) linePath.moveTo(x, y) else linePath.lineTo(x, y)
            }
            val linePaint = Paint(paintLine).apply {
                this.color = interpolateColor(fraction)
            }
            canvas.drawPath(linePath, linePaint)
        }

        for ((temp, label, color) in thresholds) {
            if (temp in minTemp..maxTemp) {
                val x = horizontalPadding
                val y = topPadding + graphHeight - (graphHeight * (temp - minTemp) / (maxTemp - minTemp))
                val paint = Paint (paintThresholdLine).apply { this.color = color }
                canvas.drawLine (x, y, width - horizontalPadding, y, paint)
                canvas.drawText (label, x + 10f, y - 10f, paintText)
            }
        }

        val linePath = Path()
        temperatureValues.forEachIndexed { index, temp ->
            val x = horizontalPadding + (drawableWidth * index / (count - 1))
            val y = topPadding + graphHeight - (graphHeight * (temp - minTemp) / (maxTemp - minTemp))
            if (index == 0) linePath.moveTo(x, y) else linePath.lineTo(x, y)
        }
        val linePaint = Paint(paintLine).apply {
            color = Color.rgb(64, 64, 64)  // 75% black (dark gray)
            strokeWidth = 4f
            style = Paint.Style.STROKE
            pathEffect = DashPathEffect(floatArrayOf(20f, 10f), 0f)
        }
        canvas.drawPath(linePath, linePaint)

        temperatureValues.forEachIndexed { index, temp ->
            val x = horizontalPadding + (drawableWidth * index / (count - 1))
            val y = topPadding + graphHeight - (graphHeight * (temp - minTemp) / (maxTemp - minTemp))
            canvas.drawLine(x, topPadding, x, height - bottomPadding, paintDashedLine)
            canvas.drawLine(x - 5, y - 5, x + 5, y + 5, paintPoint)
            canvas.drawLine(x - 5, y + 5, x + 5, y - 5, paintPoint)
            val text = String.format("%.1f°C", temp)
            val textWidth = getTextWidth(paintText, text)
            var textX = x - (textWidth / 2)
            if (textX < horizontalPadding)
                textX = horizontalPadding
            else if (textX + textWidth > width - horizontalPadding)
                textX = width - horizontalPadding - textWidth
            canvas.drawText(text, textX, y - 20, paintText)
        }
    }

    private fun getTextWidth(paint: Paint, text: String): Float {
        return paint.measureText(text)
    }
    private fun interpolateColor(fraction: Float): Int {
        return Color.rgb(255, (255 * fraction).toInt(), (255 * fraction).toInt())
    }
}