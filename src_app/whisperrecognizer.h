#ifndef WHISPERRECOGNIZER_H
#define WHISPERRECOGNIZER_H

#include <QThread>
#include <QObject>
#include <QString>
#include <QVector>

#include "whisper.h"
#include "common-sdl.h"

class WhisperRecognizer : public QThread
{
    Q_OBJECT

public:
    explicit WhisperRecognizer(QObject *parent = nullptr);
    ~WhisperRecognizer();

    void startRecognition(const QString &modelPath);
    void stopRecognition();

signals:
    void textRecognized(const QString &text);
    void waveformUpdated(const QVector<float> &waveform);

protected:
    void run() override;

private:
    bool m_isRecognitionRunning = false;
    struct whisper_context *m_ctx = nullptr;
    std::string m_modelPath;

    // Audio capture members - adapted from stream.cpp
    audio_async m_audio;
    std::vector<float> m_pcmf32;
    std::vector<float> m_pcmf32_old;
    std::vector<float> m_pcmf32_new;

    // Whisper parameters - adapted from stream.cpp
    struct whisper_params m_params;
};

#endif // WHISPERRECOGNIZER_H 