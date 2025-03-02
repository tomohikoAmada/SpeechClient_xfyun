#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include "speechclient.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onRecognitionResult(const QString& text);
    void onConnectionError(const QString& error);
    void onStatusChanged(const QString& status);

private:
    void setupUI();

    QPushButton* startButton;
    QPushButton* stopButton;
    QTextEdit* resultText;
    QLabel* statusLabel;
    SpeechClient* speechClient;
};

#endif // MAINWINDOW_H

