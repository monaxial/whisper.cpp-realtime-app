#include "waveformwidget.h"
#include <QPainter>
#include <QPen>
#include <QDebug>
#include <QWidget>
#include <QVector>
#include <QColor>
#include <QPaintEvent>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    m_pen.setColor(Qt::blue);
    m_pen.setWidth(1);
    m_backgroundColor = Qt::black;
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void WaveformWidget::updateWaveform(const QVector<float> &data)
{
    m_waveform_data = data; // Сохраняем полученные данные
    // We only need the size to trigger a repaint
    // The actual waveform data is in WhisperRecognizer and copied to m_waveform_data
    // via a mutex in the run() thread. paintEvent will read it.
    // We might need a mechanism to copy data from WhisperRecognizer in a thread-safe way
    // But for now, we assume m_waveform_data in WhisperRecognizer is updated and available.

    // Store the size (optional, for info, not drawing directly)
    // This size corresponds to the size of the buffer in WhisperRecognizer::run()
    // m_waveform_data_size = size; // If we added a member for size

    // Trigger a repaint
    // qDebug() << "Updating waveform with size:" << size; // Debug print
    this->update(); // This schedules a paintEvent
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(event->rect(), m_backgroundColor);
    
    if (m_waveform_data.empty()) {
        return;
    }
    
    painter.setPen(m_pen);
    
    qreal width = rect().width();
    qreal height = rect().height();
    
    qreal xScale = width / m_waveform_data.size();
    qreal maxAmplitude = 0.0;
    for (float sample : m_waveform_data) {
        maxAmplitude = qMax(maxAmplitude, qAbs(sample));
    }

    // Draw the waveform
    for (int i = 0; i < m_waveform_data.size() - 1; ++i) { // Итерация по QVector
        QPointF p1(
            i * xScale,
            height / 2.0 - m_waveform_data[i] * (height / 2.0 / maxAmplitude) // Центрируем и масштабируем по высоте
        );
        QPointF p2(
            (i + 1) * xScale,
            height / 2.0 - m_waveform_data[i + 1] * (height / 2.0 / maxAmplitude) // Центрируем и масштабируем по высоте
        );
        painter.drawLine(p1, p2);
    }
} 