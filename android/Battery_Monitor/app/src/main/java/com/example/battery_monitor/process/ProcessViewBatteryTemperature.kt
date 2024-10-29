package com.example.battery_monitor.process

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.View
import com.example.battery_monitor.R

class ProcessViewBatteryTemperature @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    data class Threshold(
        val temperature: Float,
        val label: String,
        val color: Int
    )

    private val paints = object {
        val line = Paint().apply {
            style = Paint.Style.STROKE
            strokeWidth = 4f
            color = context.getColor(R.color.process_temperature_graph_line_color)
        }
        val text = Paint().apply {
            textSize = 30f
            color = context.getColor(R.color.process_temperature_graph_text_color)
        }
        val dashed = Paint().apply {
            style = Paint.Style.STROKE
            strokeWidth = 2f
            color = context.getColor(R.color.process_temperature_graph_line_color)
            pathEffect = DashPathEffect(floatArrayOf(10f, 10f), 0f)
        }
        val threshold = Paint().apply {
            style = Paint.Style.STROKE
            strokeWidth = 2f
            color = context.getColor(android.R.color.holo_red_light)
        }
        val path = Paint().apply {
            style = Paint.Style.STROKE
            strokeWidth = 4f
            color = Color.rgb(64, 64, 64)
            pathEffect = DashPathEffect(floatArrayOf(20f, 10f), 0f)
        }
    }

    private val thresholds = listOf(
        Threshold(25f, "25°C", context.getColor(R.color.process_temperature_threshold_min)),
        Threshold(35f, "35°C", context.getColor(R.color.process_temperature_threshold_warn)),
        Threshold(45f, "45°C", context.getColor(R.color.process_temperature_threshold_max))
    )

    private var temperatureValues: List<Float> = emptyList()
    private val temperatureHistory = object {
        private val maxSize = 16
        private val values = mutableListOf<List<Float>>()
        fun add(temps: List<Float>) {
            values.add(0, temps)
            if (values.size > maxSize) values.removeAt(maxSize)
        }
        fun clear() = values.clear()
        val size get() = values.size
        operator fun get(index: Int) = values[index]
        fun tempRange() = values.flatten().let { temps ->
            temps.minOrNull()?.to(temps.maxOrNull()) ?: (0f to 0f)
        }
    }

    init {
        setupGestureDetector()
    }

    private fun setupGestureDetector() {
        val detector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
            override fun onDoubleTap(e: MotionEvent) = true.also { clearTemperatureValues() }
        })
        setOnTouchListener { v, event ->
            if (!detector.onTouchEvent(event)) v.performClick()
            true
        }
    }

    fun clearTemperatureValues() {
        temperatureHistory.clear()
        if (temperatureValues.isNotEmpty())
            temperatureHistory.add(temperatureValues)
        invalidate()
    }

    fun setTemperatureValues(values: List<Float>) {
        temperatureHistory.add(values)
        temperatureValues = values
        invalidate()
    }

    @SuppressLint("DrawAllocation")
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (temperatureValues.isEmpty()) return

        val dimensions = calculateDimensions()
        drawTemperatureHistory(canvas, dimensions)
        drawTemperatureThresholds(canvas, dimensions)
        drawTemperatureCurrent(canvas, dimensions)
    }

    private data class DrawDimensions(
        val width: Float,
        val height: Float,
        val paddingTop: Float = 50f,
        val paddingBottom: Float = 20f,
        val count: Int,
        val minTemp: Float,
        val maxTemp: Float
    ) {
        val paddingHorizontal = width / (count * 2)
        val drawableWidth = width - (2 * paddingHorizontal)
        val drawableHeight = height - paddingTop - paddingBottom
        fun getXPosition(index: Int) =
            paddingHorizontal + (drawableWidth * index / (count - 1))
        fun getYPosition(temp: Float) =
            paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
    }
    private fun calculateDimensions(): DrawDimensions {
        val (minTemp, maxTemp) = temperatureHistory.tempRange()
        return DrawDimensions(
            width = width.toFloat(),
            height = height.toFloat(),
            count = temperatureValues.size,
            minTemp = minTemp ?: 0f,
            maxTemp = maxTemp ?: 0f
        )
    }

    private fun drawTemperatureHistory(canvas: Canvas, dims: DrawDimensions) {
        for (i in temperatureHistory.size - 1 downTo 1) {
            val olderValues = temperatureHistory[i]
            val newerValues = temperatureHistory[i - 1]
            val olderFraction = i.toFloat() / (temperatureHistory.size - 1)
            val newerFraction = (i - 1).toFloat() / (temperatureHistory.size - 1)
            drawTemperatureHistoryArea(canvas, dims, olderValues, newerValues, olderFraction, newerFraction)
            drawTemperatureHistoryLine(canvas, dims, olderValues, olderFraction)
        }
    }

    private fun drawTemperatureHistoryArea(canvas: Canvas, dims: DrawDimensions, olderValues: List<Float>, newerValues: List<Float>, olderFraction: Float, newerFraction: Float) {
        Path().apply {
            // Draw older values path
            olderValues.forEachIndexed { index, temp ->
                val x = dims.getXPosition(index)
                val y = dims.getYPosition(temp)
                if (index == 0) moveTo(x, y) else lineTo(x, y)
            }
            // Draw newer values path in reverse
            newerValues.indices.reversed().forEach { index ->
                val x = dims.getXPosition(index)
                val y = dims.getYPosition(newerValues[index])
                lineTo(x, y)
            }
            close()
            // Draw with gradient
            canvas.drawPath(this, Paint().apply {
                style = Paint.Style.FILL
                shader = LinearGradient(
                    0f, 0f, 0f, dims.height,
                    interpolateColor(olderFraction),
                    interpolateColor(newerFraction),
                    Shader.TileMode.CLAMP
                )
            })
        }
    }

    private fun drawTemperatureHistoryLine(canvas: Canvas, dims: DrawDimensions, values: List<Float>, fraction: Float) {
        Path().apply {
            values.forEachIndexed { index, temp ->
                val x = dims.getXPosition(index)
                val y = dims.getYPosition(temp)
                if (index == 0) moveTo(x, y) else lineTo(x, y)
            }
            canvas.drawPath(this, paints.line.apply {
                color = interpolateColor(fraction)
            })
        }
    }

    private fun drawTemperatureThresholds(canvas: Canvas, dims: DrawDimensions) {
        thresholds.forEach { threshold ->
            if (threshold.temperature in dims.minTemp..dims.maxTemp) {
                val y = dims.getYPosition(threshold.temperature)
                canvas.drawLine(
                    dims.paddingHorizontal,
                    y,
                    dims.width - dims.paddingHorizontal,
                    y,
                    paints.threshold.apply { color = threshold.color }
                )
                canvas.drawText(
                    threshold.label,
                    dims.paddingHorizontal + 10f,
                    y - 10f,
                    paints.text
                )
            }
        }
    }

    private fun drawTemperatureCurrent(canvas: Canvas, dims: DrawDimensions) {
        // Draw current temperature path
        Path().apply {
            temperatureValues.forEachIndexed { index, temp ->
                val x = dims.getXPosition(index)
                val y = dims.getYPosition(temp)
                if (index == 0) moveTo(x, y) else lineTo(x, y)
            }
            canvas.drawPath(this, paints.path)
        }
        // Draw temperature labels and vertical lines
        temperatureValues.forEachIndexed { index, temp ->
            val x = dims.getXPosition(index)
            val y = dims.getYPosition(temp)
            // Draw vertical dashed line
            canvas.drawLine(
                x,
                dims.paddingTop,
                x,
                dims.height - dims.paddingBottom,
                paints.dashed
            )
            // Draw temperature text
            val text = "%.1f°C".format(temp)
            val textWidth = paints.text.measureText(text)
            val textX = (x - (textWidth / 2)).coerceIn(
                dims.paddingHorizontal,
                dims.width - dims.paddingHorizontal - textWidth
            )
            canvas.drawText(text, textX, y - 20, paints.text)
        }
    }

    private fun interpolateColor(fraction: Float): Int =
        Color.rgb(255, (255 * fraction).toInt(), (255 * fraction).toInt())
}