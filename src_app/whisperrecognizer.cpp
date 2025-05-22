#include "whisperrecognizer.h"
#include "common.h"
#include "common-whisper.h"

#include <QDebug>
#include <QElapsedTimer>
#include <thread>
#include <string>
#include <vector>

// Статические колбэки для SDL Audio
namespace {
    audio_async * g_audio_async = nullptr;

    void audio_callback(void * userdata, uint8_t * stream, int len)
    {
        g_audio_async->callback(stream, len);
    }
}

// Adapted from stream.cpp
struct whisper_params get_default_params() {
    struct whisper_params params;
    // Настройки параметров аудиозахвата и модели по умолчанию
    params.step_ms    = 3000;
    params.length_ms  = 10000;
    params.keep_ms    = 200;
    params.capture_id = -1; // Default audio device
    params.model     = "models/ggml-small.bin"; // Use a multilingual model

    return params;
}

WhisperRecognizer::WhisperRecognizer(QObject *parent)
    : QThread(parent),
    // Используем значения step_ms и length_ms из m_params, которые инициализируются get_default_params
      m_audio(get_default_params().step_ms + get_default_params().length_ms)
{
    m_ctx = nullptr; // Инициализируем указатель нулевым значением
    m_isRecognitionRunning = false;
    m_params = get_default_params(); // Инициализируем m_params

    // Инициализируем SDL субсистемы здесь, если они еще не инициализированы где-то централизованно
    // В противном случае, если SDL_Init уже вызывался, повторный вызов безопасен.
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        qCritical() << "Не удалось инициализировать SDL Audio:" << SDL_GetError();
        // Возможно, следует сообщить об ошибке или выбросить исключение
    }
}

WhisperRecognizer::~WhisperRecognizer()
{
    // Поток завершится при завершении функции run()
    stopRecognition(); // Остановка потока
    wait(); // Ждем завершения потока

    freeWhisper();
}

void WhisperRecognizer::startRecognition(const QString &modelPath)
{
    if (m_isRecognitionRunning) {
        qDebug() << "Recognition is already running.";
        return;
    }

    // Сохраняем путь к модели
    m_modelPath = modelPath.toStdString();

    // Инициализация whisper и аудио
    if (!initWhisper()) {
        qDebug() << "Failed to initialize Whisper.";
        return;
    }

    if (!initAudio()) {
        freeWhisper();
        qDebug() << "Failed to initialize audio.";
        return;
    }

    // Устанавливаем флаг и запускаем поток
    m_isRecognitionRunning = true;
    start(); // Запуск QThread::run()
}

void WhisperRecognizer::stopRecognition()
{
    if (!m_isRecognitionRunning) {
        qDebug() << "Recognition is not running.";
        return;
    }

    // Сбрасываем флаг
    m_isRecognitionRunning = false;

    // Остановка аудиозахвата
    m_audio.pause();

    qDebug() << "Recognition stopped.";
}

bool WhisperRecognizer::isRunning() const
{
    return m_isRecognitionRunning;
}

void WhisperRecognizer::run()
{
    qDebug() << "Whisper recognition thread started.";

    int n_samples = 0; // Общее количество обработанных семплов (для отслеживания смещения)

    const int n_samples_step = (1e-3 * m_params.step_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_len  = (1e-3 * m_params.length_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3 * m_params.keep_ms) * WHISPER_SAMPLE_RATE;

    //std::vector<float> pcmf32; // Локальный буфер для аудиоданных - БОЛЬШЕ НЕ ИСПОЛЬЗУЕМ ТАК
    //std::vector<float> temp_buffer; // Временный буфер для захваченных данных - БОЛЬШЕ НЕ ИСПОЛЬЗУЕМ ТАК

    // Инициализация буферов перекрытия
    m_pcmf32_old.clear();
    m_pcmf32_new.clear();

    while (m_isRecognitionRunning) {
        // Захват аудио (захватываем step_ms данных)
        // Используем step_ms для получения небольших порций новых данных
        m_audio.get(m_params.step_ms, m_pcmf32_new);

        if (m_pcmf32_new.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Логика буферизации с перекрытием, как в stream.cpp
        // take up to params.length_ms audio from previous iteration
        const int n_samples_take = std::min((int) m_pcmf32_old.size(), std::max(0, n_samples_keep + n_samples_len - (int)m_pcmf32_new.size()));

        std::vector<float> pcmf32_processing; // Буфер для текущей обработки
        pcmf32_processing.resize(m_pcmf32_new.size() + n_samples_take);

        for (int i = 0; i < n_samples_take; i++) {
            pcmf32_processing[i] = m_pcmf32_old[m_pcmf32_old.size() - n_samples_take + i];
        }

        memcpy(pcmf32_processing.data() + n_samples_take, m_pcmf32_new.data(), m_pcmf32_new.size()*sizeof(float));

        // Обновляем буфер старых данных для следующей итерации
        m_pcmf32_old = pcmf32_processing;

        // copy to waveform data (using the processing buffer)
        {
            // Создаем QVector для передачи через сигнал
            QVector<float> waveform_qvector;
            waveform_qvector.reserve(pcmf32_processing.size());
            for (float sample : pcmf32_processing) {
                waveform_qvector.append(sample);
            }

            emit waveformUpdated(waveform_qvector); // Передаем QVector<float> для обновления
        }

        // process frames
        // Теперь условие зависит от размера буфера для обработки
        // В stream.cpp обработка идет порциями step_ms, но whisper_full может работать с length_ms
        // Попробуем обрабатывать порциями length_ms, как в stream.cpp, когда буфер обработки достаточно большой

        qDebug() << "pcmf32_processing.size():" << pcmf32_processing.size() << ", n_samples_len:" << n_samples_len; // DEBUG: Размер буфера и порог

        if ((int)pcmf32_processing.size() >= n_samples_len) {

            qDebug() << "Calling whisper_full..."; // DEBUG: Вызов whisper_full

            // initialize whisper_full_params for greedy sampling
            struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

            // Настройки параметров распознавания из m_params и хардкода
            wparams.n_threads       = m_params.n_threads; // Используем n_threads из m_params
            wparams.language        = "auto"; // Всегда auto-detect язык
            wparams.translate     = false; // Пока отключаем перевод
            wparams.no_context    = true; // Пока не сохраняем контекст
            wparams.no_timestamps = true; // Без временных меток
            wparams.single_segment  = true; // Обрабатываем как один сегмент
            wparams.print_special   = false; // Не печатаем спецтокены
            wparams.print_progress  = false;
            wparams.print_realtime  = false;
            wparams.print_timestamps = false;
            wparams.token_timestamps = false;
            wparams.max_len = 0;
            // Дополнительные параметры, которые могут быть нужны для качества:
            wparams.temperature = 0.8f;
            wparams.temperature_inc = 0.2f;
            wparams.logprob_thold = -1.0f;
            wparams.no_speech_thold = 0.6f;
            // VAD параметры (используются в whisper_full, если включен VAD)
            // wparams.vad_thold = m_params.vad_thold;
            // wparams.freq_thold = m_params.freq_thold;

            // Отключаем VAD, как в оригинальном stream.cpp
            wparams.vad = false;

            // Beam search параметры (используются если WHISPER_SAMPLING_BEAM_SEARCH)
            // wparams.beam_search.beam_size = ...
            // wparams.beam_search.patience = ...

            // [TDRZ] tinydiarize (если модель поддерживает)
            // wparams.tdrz_enable = m_params.tinydiarize; // Если tinydiarize настраивается

            // Вызов whisper_full на буфере обработки
            // Передаем контекст whisper, параметры и данные аудио
            if (whisper_full(m_ctx, wparams, pcmf32_processing.data(), pcmf32_processing.size()) != 0) {
                fprintf(stderr, "failed to process audio\\n");
                // continue; // Не пропускаем итерацию, чтобы не потерять данные
            } else {
                // extract texts from the new segments
                const int n_segments = whisper_full_n_segments(m_ctx);
                qDebug() << "whisper_full returned, n_segments:" << n_segments; // DEBUG: Количество сегментов

                for (int i = 0; i < n_segments; i++) {
                    const char * text = whisper_full_get_segment_text(m_ctx, i);
                    qDebug() << "Recognized text segment:" << QString(text); // DEBUG: Выводим распознанный текст

                    emit textRecognized(QString(text));
                }
            }

            // Обновляем общее количество обработанных семплов
            n_samples += pcmf32_processing.size() - n_samples_take; // Учитываем только новые семплы

            // Очищаем буфер новых данных после использования
            m_pcmf32_new.clear();
        }

        // Небольшая задержка, чтобы не перегружать процессор
        QThread::msleep(10);
    }

    // process any remaining audio (если нужно обработать остаток после остановки)
    // ЭТОТ БЛОК СЕЙЧАС НЕ АКТИВЕН В НАШЕЙ ЛОГИКЕ, Т.К. МЫ НЕ УДАЛЯЕМ СЕМПЛЫ
    /*
    if (!pcmf32.empty()) {
        // ... логика обработки остатка ...
    }
    */


    // Очистка буферов после завершения распознавания
    m_pcmf32_old.clear();
    m_pcmf32_new.clear();
    // pcmf32_processing локальный и будет удален при выходе из scope

    qDebug() << "Whisper recognition thread finished.";
}

bool WhisperRecognizer::initWhisper()
{
    // Загрузка модели whisper
    // TODO: Handle model loading errors
    whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(m_modelPath.c_str(), cparams);

    if (m_ctx == nullptr) {
        qDebug() << "Failed to load whisper model from" << QString::fromStdString(m_modelPath);
        return false;
    }

    qDebug() << "Whisper model loaded successfully.";
    return true;
}

void WhisperRecognizer::freeWhisper()
{
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
        qDebug() << "Whisper context freed.";
    }
}

bool WhisperRecognizer::initAudio()
{
    // Инициализация SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        qDebug() << "SDL could not initialize! SDL_Error:" << SDL_GetError();
        return false;
    }

    // TODO: Реализовать выбор аудиоустройства
    // Пока используем устройство по умолчанию

    // Инициализация m_audio с нужной длиной буфера уже произошла в конструкторе
    // Теперь инициализируем аудиозахват
    if (!m_audio.init(m_params.capture_id, WHISPER_SAMPLE_RATE)) {
         qDebug() << "Failed to initialize audio capture.";
         return false;
    }
    
    // Запускаем аудиозахват
    if (!m_audio.resume()) {
         qDebug() << "Failed to resume audio capture.";
         // TODO: Handle error, maybe call m_audio.free() and return false
    }

    qDebug() << "Audio initialized successfully.";
    return true;
}

void WhisperRecognizer::freeAudio()
{
    // Деинициализация audio_async происходит автоматически в деструкторе
    // Деинициализация SDL
    SDL_Quit();
    qDebug() << "Audio freed.";
}

// TODO: Реализовать функцию processAudio() если потребуется отдельная обработка
// void WhisperRecognizer::processAudio()
// {
//     // Логика обработки аудио
// } 