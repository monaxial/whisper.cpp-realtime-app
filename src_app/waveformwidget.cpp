#include "waveformwidget.h"

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    m_pen.setColor(Qt::blue);
    m_pen.setWidth(1);
    m_backgroundColor = Qt::black;
    setMinimumHeight(100);
}

void WaveformWidget::updateWaveform(const QVector<float> &waveform)
{
    m_waveform = waveform;
    update(); // Request repaint
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.fillRect(rect(), m_backgroundColor);
    
    if (m_waveform.isEmpty()) {
        return;
    }
    
    painter.setPen(m_pen);
    
    const int width = this->width();
    const int height = this->height();
    
    // Simple waveform visualization
    QPointF prevPoint;
    for (int i = 0; i < m_waveform.size(); ++i) {
        qreal x = (qreal)i / (m_waveform.size() - 1) * width;
        // Scale sample to widget height, centering at half height
        qreal y = height / 2.0 - m_waveform[i] * (height / 2.0);
        
        QPointF currentPoint(x, y);
        
        if (i > 0) {
            painter.drawLine(prevPoint, currentPoint);
        }
        prevPoint = currentPoint;
    }
} 