#ifndef SPEECHCLIENT_H
#define SPEECHCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QAudioSource>
#include <QByteArray>
#include <QTimer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QMediaCaptureSession>
#include <QAudioInput>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QBuffer>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QDateTime>
#include <QUuid>

#include <QCryptographicHash>

#ifdef Q_OS_MAC
#include <CommonCrypto/CommonHMAC.h>
#endif

class SpeechClient : public QObject
{
    Q_OBJECT

public:
    explicit SpeechClient(QObject *parent = nullptr);
    ~SpeechClient();

    void startRecognition();
    void stopRecognition();

signals:
    void recognitionResult(const QString& text);
    void connectionError(const QString& error);
    void statusChanged(const QString& status);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onBinaryMessageReceived(const QByteArray& message);
    void onError(QAbstractSocket::SocketError error);
    void onAudioDataReady();
    void sendKeepAlive();

private:
    void init();
    void initWebSocket();
    void initAudioInput();
    QString generateAuthUrl();
    void handleRecognitionResult(const QJsonObject& result);

    QWebSocket m_webSocket;
    QAudioSource* m_audioSource;
    QBuffer* m_audioBuffer;
    bool m_isRecording;

    QNetworkAccessManager* m_networkManager;
    void handleHandshakeResponse(QNetworkReply* reply);

    QTimer* m_keepAliveTimer;

    QIODevice* m_audioDevice;

    // 讯飞 API 认证信息 - WebAPI 只需要 APIKey 和 APISecret
    const QString API_KEY = "xxx";
    const QString API_SECRET = "xxx";

    // WebSocket 连接参数
    const QString BASE_URL = "wss://iat-api.xfyun.cn/v2/iat";
    const QString HOST = "iat-api.xfyun.cn";

};

#endif // SPEECHCLIENT_H
