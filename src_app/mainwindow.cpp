#include "mainwindow.h"
#include <QDebug>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Whisper Real-time Recognizer");
    
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    startStopButton = new QPushButton("Start Recognition", this);
    connect(startStopButton, &QPushButton::clicked, this, &MainWindow::onStartStopButtonClicked);
    
    recognizedTextLabel = new QLabel("Recognized Text: ", this);
    recognizedTextLabel->setWordWrap(true);

    waveformWidget = new WaveformWidget(this);
    
    layout->addWidget(startStopButton);
    layout->addWidget(recognizedTextLabel);
    layout->addWidget(waveformWidget);
    
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    recognizer = new WhisperRecognizer(this);
    connect(recognizer, &WhisperRecognizer::textRecognized, this, &MainWindow::onTextRecognized);
    connect(recognizer, &WhisperRecognizer::waveformUpdated, this, &MainWindow::onWaveformUpdated);
}

MainWindow::~MainWindow()
{
    if (recognizer->isRunning()) {
        recognizer->stopRecognition();
        recognizer->wait();
    }
    delete recognizer;
}

void MainWindow::onStartStopButtonClicked()
{
    if (!isRecognitionActive) {
        // Start recognition
        QString modelPath = "models/ggml-base.ru.bin"; // TODO: Make this configurable
        recognizer->startRecognition(modelPath);
        startStopButton->setText("Stop Recognition");
        isRecognitionActive = true;
        recognizedTextLabel->setText("Recognized Text: "); // Clear previous text
    } else {
        // Stop recognition
        recognizer->stopRecognition();
        startStopButton->setText("Start Recognition");
        isRecognitionActive = false;
    }
}

void MainWindow::onTextRecognized(const QString &text)
{
    recognizedTextLabel->setText(recognizedTextLabel->text() + text);
    
    // Insert text into active window using xdotool
    QProcess::startDetached("xdotool", QStringList() << "type" << text);
}

void MainWindow::onWaveformUpdated(const QVector<float> &waveform)
{
    waveformWidget->updateWaveform(waveform);
} 