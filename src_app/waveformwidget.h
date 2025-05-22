#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPainter>
#include <QPen>
#include <QColor>

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);
    
public slots:
    void updateWaveform(const QVector<float> &waveform);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    QVector<float> m_waveform;
    QPen m_pen;
    QColor m_backgroundColor;
};

#endif // WAVEFORMWIDGET_H 