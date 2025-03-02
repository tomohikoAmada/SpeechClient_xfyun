#include "mainwindow.h"
#include <QVBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    speechClient = new SpeechClient(this);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
    connect(speechClient, &SpeechClient::recognitionResult, this, &MainWindow::onRecognitionResult);
    connect(speechClient, &SpeechClient::connectionError, this, &MainWindow::onConnectionError);
    connect(speechClient, &SpeechClient::statusChanged, this, &MainWindow::onStatusChanged);
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* layout = new QVBoxLayout(centralWidget);

    startButton = new QPushButton("开始识别", this);
    stopButton = new QPushButton("停止识别", this);
    stopButton->setEnabled(false);

    resultText = new QTextEdit(this);
    resultText->setReadOnly(true);

    statusLabel = new QLabel("就绪", this);

    layout->addWidget(startButton);
    layout->addWidget(stopButton);
    layout->addWidget(resultText);
    layout->addWidget(statusLabel);

    setMinimumSize(600, 400);
    setWindowTitle("科大讯飞语音识别");
}

void MainWindow::onStartButtonClicked()
{
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    resultText->clear();
    speechClient->startRecognition();
}

void MainWindow::onStopButtonClicked()
{
    stopButton->setEnabled(false);
    startButton->setEnabled(true);
    speechClient->stopRecognition();
}

void MainWindow::onRecognitionResult(const QString& text)
{
    resultText->append(text);
}

void MainWindow::onConnectionError(const QString& error)
{
    QMessageBox::warning(this, "错误", error);
    stopButton->setEnabled(false);
    startButton->setEnabled(true);
}

void MainWindow::onStatusChanged(const QString& status)
{
    statusLabel->setText(status);
}
