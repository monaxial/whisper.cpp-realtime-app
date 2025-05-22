#ifndef WHISPERRECOGNIZER_H
#define WHISPERRECOGNIZER_H

#include <QThread>
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include "whisper.h"
#include "common-sdl.h"
#include "common-whisper.h"
#include <SDL.h>
#include <SDL_audio.h>

// Definition of whisper_params for audio capture, model path, and threads
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency()); // Количество потоков
    int32_t step_ms    = 3000;
    int32_t length_ms  = 10000;
    int32_t keep_ms    = 200;
    int32_t capture_id = -1;

    std::string model     = "models/ggml-small.bin"; // Use a multilingual model
    // std::string fname_out; // Не используем сохранение аудио в этом приложении
};

class WhisperRecognizer : public QThread
{
    Q_OBJECT

public:
    explicit WhisperRecognizer(QObject *parent = nullptr);
    ~WhisperRecognizer();

    void startRecognition(const QString &modelPath);
    void stopRecognition();

    bool isRunning() const;

signals:
    void textRecognized(const QString &text);
    void waveformUpdated(const QVector<float> &data);

protected:
    void run() override;

private:
    std::atomic_bool m_isRecognitionRunning = false;
    struct whisper_context *m_ctx;
    std::string m_modelPath;

    // Audio capture members - adapted from stream.cpp
    audio_async m_audio;
    std::vector<float> m_pcmf32;
    std::vector<float> m_pcmf32_old;
    std::vector<float> m_pcmf32_new;

    // Whisper parameters - adapted from stream.cpp
    struct whisper_params m_params;

    // Audio device and buffer members
    SDL_AudioDeviceID m_audio_device = 0;
    SDL_AudioSpec m_audio_spec;
    std::vector<float> m_audio_buffer;

    // Private methods for initialization and processing
    bool initWhisper();
    void freeWhisper();
    bool initAudio();
    void freeAudio();
};

#endif // WHISPERRECOGNIZER_H