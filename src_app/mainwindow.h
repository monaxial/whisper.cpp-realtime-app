#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>

#include "whisperrecognizer.h"
#include "waveformwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartStopButtonClicked();
    void onTextRecognized(const QString &text);
    void onWaveformUpdated(const QVector<float> &waveform);

private:
    QPushButton *startStopButton;
    QLabel *recognizedTextLabel;
    WaveformWidget *waveformWidget;
    WhisperRecognizer *recognizer;

    bool isRecognitionActive = false;

    // Add other UI elements here later
};

#endif // MAINWINDOW_H 