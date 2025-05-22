#include "mainwindow.h"
//#include "ui_mainwindow.h"
#include "whisperrecognizer.h"
#include "waveformwidget.h"

#include <QPushButton>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QStyle>
#include <QProcess>

// Для инициализации SDL Audio в главном потоке, если это необходимо.
// Хотя мы инициализируем SDL Audio в потоке WhisperRecognizer,
// возможно, потребуется инициализация SDL_INIT_VIDEO или других субсистем в главном потоке.
// Для простоты пока оставим это на усмотрение пользователя или добавим позже.
// #include <SDL.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createUi();
}

MainWindow::~MainWindow()
{
    // Убедимся, что поток распознавания остановлен перед удалением
    recognizer->stopRecognition();
    // QObject деструктор автоматически удалит дочерние объекты (recognizer, startStopButton, etc.)
}

void MainWindow::createUi()
{
    // Создаем элементы UI
    startStopButton = new QPushButton("Start Recognition", this);
    recognizedTextEdit = new QPlainTextEdit(this);
    waveformWidget = new WaveformWidget(this);

    // Настраиваем элементы UI
    recognizedTextEdit->setReadOnly(true); // Текст только для чтения

    // Создаем компоновщики
    QVBoxLayout *layout = new QVBoxLayout();
    QHBoxLayout *horizontalLayout = new QHBoxLayout();

    // Добавляем элементы в компоновщики
    horizontalLayout->addWidget(startStopButton);
    // Добавляем другие элементы управления, если они будут

    layout->addLayout(horizontalLayout);
    layout->addWidget(waveformWidget, 1); // Виджет волны занимает больше места
    layout->addWidget(recognizedTextEdit, 2); // Поле текста занимает еще больше места

    // Создаем центральный виджет и устанавливаем компоновщик
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    // Создаем экземпляр WhisperRecognizer
    recognizer = new WhisperRecognizer(this);

    // Соединяем сигналы и слоты
    connect(startStopButton, &QPushButton::clicked, this, &MainWindow::onStartStopButtonClicked);
    connect(recognizer, &WhisperRecognizer::textRecognized, this, &MainWindow::onTextRecognized);
    connect(recognizer, SIGNAL(waveformUpdated(const QVector<float> &)),
            waveformWidget, SLOT(updateWaveform(const QVector<float> &)));

    // Дополнительные настройки или инициализация
    // Например, установка иконки для кнопки
    startStopButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void MainWindow::onStartStopButtonClicked()
{
    if (recognizer->isRunning()) {
        // Если распознавание запущено, останавливаем его
        recognizer->stopRecognition();
        startStopButton->setText("Start Recognition");
        startStopButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        qDebug() << "Распознавание остановлено по кнопке.";
    } else {
        // Если распознавание остановлено, запускаем его
        // !!! Здесь нужно указать путь к модели whisper !!!
        // Пока используем заглушку "ggml-base.en.bin", предполагая, что она в текущей директории или путях поиска
        recognizer->startRecognition("../models/ggml-base.en.bin"); // Передаем относительный путь к модели
        startStopButton->setText("Stop Recognition");
        startStopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
        recognizedTextEdit->clear(); // Очищаем поле текста при новом запуске
        recognizedTextEdit->appendPlainText("--- Распознавание запущено ---"); // Вставляем тестовую строку
        qDebug() << "Распознавание запущено по кнопке.";
    }
}

void MainWindow::onTextRecognized(const QString &text)
{
    qDebug() << "Received recognized text:" << text; // DEBUG: Проверяем, что приходит в слот

    // Trim whitespace from the recognized text
    QString trimmedText = text.trimmed();

    // Append recognized text to the text edit
    // Only append if the trimmed text is not empty
    if (!trimmedText.isEmpty()) {
        recognizedTextEdit->appendPlainText(trimmedText); // Используем appendPlainText для добавления текста
        qDebug() << "Appended trimmed text to QPlainTextEdit:" << trimmedText; // DEBUG: Подтверждаем выполнение appendPlainText с обрезанным текстом
        recognizedTextEdit->ensureCursorVisible(); // Прокручиваем к последней строке
    } else {
        qDebug() << "Recognized text is empty or only whitespace after trimming. Not appending.";
    }

    // TODO: Вставить текст в активное окно с помощью xdotool
    // Для тестирования пока просто выводим в консоль
    // QProcess::startDetached("xdotool type \"" + text + "\""); // Keep original text for xdotool if needed later
} 