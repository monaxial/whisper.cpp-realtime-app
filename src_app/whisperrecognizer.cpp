#include "whisperrecognizer.h"
#include "common.h"
#include "common-whisper.h"

#include <QDebug>
#include <QElapsedTimer>

// Adapted from stream.cpp
struct whisper_params get_default_params() {
    struct whisper_params params;
    params.n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    params.step_ms    = 3000;
    params.length_ms  = 10000;
    params.keep_ms    = 200;
    params.capture_id = -1; // Default audio device
    params.max_tokens = 32;
    params.audio_ctx  = 0;
    params.beam_size  = -1;

    params.vad_thold    = 0.6f;
    params.freq_thold   = 100.0f;

    params.translate     = false;
    params.no_fallback   = false;
    params.print_special = false;
    params.no_context    = true; // Keep context off for now
    params.no_timestamps = true; // No timestamps for real-time output
    params.tinydiarize   = false;
    params.save_audio    = false;
    params.use_gpu       = true;
    params.flash_attn    = false;

    params.language  = "ru"; // Default to Russian
    params.model     = "models/ggml-base.ru.bin";
    params.fname_out = "";

    return params;
}

WhisperRecognizer::WhisperRecognizer(QObject *parent)
    : QThread(parent),
      m_audio(get_default_params().length_ms) // Initialize audio with default length
{
    m_params = get_default_params();
}

WhisperRecognizer::~WhisperRecognizer()
{
    stopRecognition();
    wait();
    if (m_ctx) {
        whisper_free(m_ctx);
    }
}

void WhisperRecognizer::startRecognition(const QString &modelPath)
{
    if (m_isRecognitionRunning) {
        qDebug() << "Recognition is already running.";
        return;
    }

    m_modelPath = modelPath.toStdString();

    // Initialize audio capture
    if (!m_audio.init(m_params.capture_id, WHISPER_SAMPLE_RATE)) {
        qDebug() << "Failed to initialize audio capture!";
        emit textRecognized("Error: Failed to initialize audio capture.");
        return;
    }
    m_audio.resume();

    // Initialize whisper context
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = m_params.use_gpu;
    cparams.flash_attn = m_params.flash_attn;

    m_ctx = whisper_init_from_file_with_params(m_modelPath.c_str(), cparams);
    if (!m_ctx) {
        qDebug() << "Failed to initialize whisper context!";
        emit textRecognized("Error: Failed to initialize whisper context. Make sure the model path is correct.");
        m_audio.pause();
        return;
    }

    m_isRecognitionRunning = true;
    start(); // Start the thread
}

void WhisperRecognizer::stopRecognition()
{
    m_isRecognitionRunning = false;
    m_audio.pause();
}

void WhisperRecognizer::run()
{
    const int n_samples_step = (1e-3 * m_params.step_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_len  = (1e-3 * m_params.length_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3 * m_params.keep_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_30s  = (1e-3 * 30000.0) * WHISPER_SAMPLE_RATE;

    const bool use_vad = n_samples_step <= 0;

    m_pcmf32.resize(n_samples_30s, 0.0f);
    m_pcmf32_new.resize(n_samples_30s, 0.0f);

    QElapsedTimer timer;
    timer.start();

    while (m_isRecognitionRunning) {
        // Process new audio
        if (!use_vad) {
            // Sliding window mode
            m_audio.get(m_params.step_ms, m_pcmf32_new);

            if ((int) m_pcmf32_new.size() > 2 * n_samples_step) {
                 qDebug() << "WARNING: cannot process audio fast enough, dropping audio ...";
                 m_audio.clear();
                 continue;
            }

            if ((int) m_pcmf32_new.size() >= n_samples_step) {
                m_audio.clear();

                const int n_samples_new = m_pcmf32_new.size();
                const int n_samples_take = std::min((int) m_pcmf32_old.size(), std::max(0, n_samples_keep + n_samples_len - n_samples_new));

                m_pcmf32.resize(n_samples_new + n_samples_take);

                for (int i = 0; i < n_samples_take; i++) {
                    m_pcmf32[i] = m_pcmf32_old[m_pcmf32_old.size() - n_samples_take + i];
                }

                memcpy(m_pcmf32.data() + n_samples_take, m_pcmf32_new.data(), n_samples_new * sizeof(float));

                m_pcmf32_old = m_pcmf32;

                // Emit waveform data for visualization (simple copy for now)
                emit waveformUpdated(QVector<float>::fromStdVector(m_pcmf32));

                // Run the inference
                whisper_full_params wparams = whisper_full_default_params(m_params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY);

                wparams.print_progress   = false;
                wparams.print_special    = m_params.print_special;
                wparams.print_realtime   = false;
                wparams.print_timestamps = !m_params.no_timestamps;
                wparams.translate        = m_params.translate;
                wparams.single_segment   = !use_vad;
                wparams.max_tokens       = m_params.max_tokens;
                wparams.language         = m_params.language.c_str();
                wparams.n_threads        = m_params.n_threads;
                wparams.beam_search.beam_size = m_params.beam_size;
                wparams.audio_ctx        = m_params.audio_ctx;
                wparams.tdrz_enable      = m_params.tinydiarize;
                wparams.temperature_inc  = m_params.no_fallback ? 0.0f : wparams.temperature_inc;
                // Prompt tokens omitted for simplicity (no context)

                if (whisper_full(m_ctx, wparams, m_pcmf32.data(), m_pcmf32.size()) != 0) {
                    qDebug() << "Failed to process audio";
                } else {
                    // Print result
                    const int n_segments = whisper_full_n_segments(m_ctx);
                    for (int i = 0; i < n_segments; ++i) {
                        const char * text = whisper_full_get_segment_text(m_ctx, i);
                        emit textRecognized(QString::fromUtf8(text));
                    }
                }
            }

        } else {
            // VAD mode (simplified)
            // This part would need more careful adaptation from stream.cpp if VAD is required.
            // For simplicity, we focus on the sliding window approach for now.
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Prevent high CPU usage if VAD not implemented
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small sleep to yield CPU
    }

    qDebug() << "Recognition thread stopped.";
    m_audio.pause();
    // whisper_free(m_ctx); // Free context in destructor
    m_ctx = nullptr; // Set to null after freeing
} 