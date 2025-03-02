#include "speechclient.h"
#include <QUrlQuery>
#include <QJsonArray>
#include <QDebug>

#if QT_CONFIG(permissions)
#include <QCoreApplication>
#include <QPermission>
#endif

#include <QMessageAuthenticationCode>

QByteArray hmacSha256(const QByteArray& key, const QByteArray& message) {
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(key);
    code.addData(message);
    return code.result();
}

SpeechClient::SpeechClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_isRecording(false)
    , m_audioSource(nullptr)
    , m_audioBuffer(nullptr)
    , m_keepAliveTimer(nullptr)
{
    initWebSocket();
    init();

    m_keepAliveTimer = new QTimer(this);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &SpeechClient::sendKeepAlive);
    m_keepAliveTimer->start(15000);
}

void SpeechClient::sendKeepAlive()
{
    if (m_isRecording && m_webSocket.state() == QAbstractSocket::ConnectedState) {
        QJsonObject frame;
        QJsonObject dataObj;
        dataObj["status"] = 1;
        dataObj["format"] = "audio/L16;rate=16000";
        dataObj["encoding"] = "raw";
        dataObj["audio"] = "";
        frame["data"] = dataObj;

        QString frameStr = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        m_webSocket.sendTextMessage(frameStr);
    }
}

SpeechClient::~SpeechClient()
{
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
    }
    if (m_audioBuffer) {
        delete m_audioBuffer;
    }
    if (m_networkManager) {
        delete m_networkManager;
    }
}

void SpeechClient::init()
{
#if QT_CONFIG(permissions)
    QMicrophonePermission microphonePermission;
    switch (qApp->checkPermission(microphonePermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(microphonePermission, this, &SpeechClient::init);
        return;
    case Qt::PermissionStatus::Denied:
        qWarning("Microphone permission is not granted!");
        emit connectionError("麦克风权限未获授权，语音识别无法工作");
        return;
    case Qt::PermissionStatus::Granted:
        qDebug() << "Microphone permission granted!";
        break;
    }
#endif

    initAudioInput();
}

void SpeechClient::initWebSocket()
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    m_webSocket.setSslConfiguration(sslConfig);

    connect(&m_webSocket, &QWebSocket::connected, this, &SpeechClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &SpeechClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &SpeechClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &SpeechClient::onBinaryMessageReceived);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &SpeechClient::onError);

    connect(&m_webSocket, &QWebSocket::sslErrors,
            this, [](const QList<QSslError>& errors) {
                qDebug() << "WebSocket SSL Errors:";
                for(const auto& error : errors) {
                    qDebug() << error.errorString();
                }
            });
}

void SpeechClient::initAudioInput()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    qDebug() << "Audio format - sample rate:" << format.sampleRate();
    qDebug() << "Audio format - channel count:" << format.channelCount();
    qDebug() << "Audio format - sample size:" << format.bytesPerSample() * 8;

    qDebug() << "Available audio input devices:";
    for (const QAudioDevice &device : QMediaDevices::audioInputs())
        qDebug() << " - " << device.description();

    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isNull()) {
        qDebug() << "Using audio input device:" << inputDevice.description();
        qDebug() << "Device is valid:" << !inputDevice.isNull();
        qDebug() << "Format supported:" << inputDevice.isFormatSupported(format);

        QList<QAudioFormat::SampleFormat> sampleFormats = inputDevice.supportedSampleFormats();

        qDebug() << "Supported sample formats:" << sampleFormats;

        m_audioSource = new QAudioSource(inputDevice, format, this);
        m_audioSource->setVolume(1.0);
        m_audioSource->setBufferSize(8000);
    } else {
        qDebug() << "Error: No suitable audio input device found";
        emit connectionError("找不到合适的音频输入设备");
    }
}

QString SpeechClient::generateAuthUrl()
{
    QString baseUrl = "wss://iat-api.xfyun.cn/v2/iat";
    QUrl url(baseUrl);

    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = now.toString("ddd, dd MMM yyyy HH:mm:ss") + " GMT";

    QString signatureOrigin = QString("host: %1\n"
                                      "date: %2\n"
                                      "GET /v2/iat HTTP/1.1")
                                  .arg(HOST)
                                  .arg(date);

    qDebug() << "Signature Origin String:" << signatureOrigin;

    QByteArray secretKey = API_SECRET.toUtf8();
    QByteArray signatureSha = hmacSha256(secretKey, signatureOrigin.toUtf8());
    QString signature = signatureSha.toBase64();

    qDebug() << "Signature:" << signature;

    QString authorizationOrigin = QString("api_key=\"%1\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%2\"")
                                      .arg(API_KEY)
                                      .arg(signature);

    qDebug() << "Authorization Origin:" << authorizationOrigin;

    QString authorization = authorizationOrigin.toUtf8().toBase64();

    qDebug() << "Final Authorization:" << authorization;

    QUrlQuery query;
    query.addQueryItem("authorization", authorization);
    query.addQueryItem("date", date);
    query.addQueryItem("host", HOST);

    url.setQuery(query);

    qDebug() << "Final URL:" << url.toString();
    qDebug() << "最终生成的签名原始字符串:" << signatureOrigin;
    qDebug() << "生成的 signature:" << signature;
    qDebug() << "完整的 authorization 字符串(base64之前):" << authorizationOrigin;

    return url.toString();
}

void SpeechClient::startRecognition()
{
    if (!m_audioSource || m_audioSource->error() != QAudio::NoError) {
        qDebug() << "Audio source error:" << (m_audioSource ? m_audioSource->error() : QAudio::OpenError);
        emit connectionError("音频设备错误");
        return;
    }

    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = now.toString("ddd, dd MMM yyyy HH:mm:ss") + " GMT";

    QString wsUrlString = generateAuthUrl();
    QUrl wsUrl(wsUrlString);

    QUrl httpUrl = wsUrl;
    httpUrl.setScheme("https");

    qDebug() << "WebSocket URL:" << wsUrl.toString();
    qDebug() << "Initial HTTPS URL:" << httpUrl.toString();

    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }

    QNetworkRequest request(httpUrl);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    request.setRawHeader("Host", HOST.toUtf8());
    request.setRawHeader("Date", date.toUtf8());
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Upgrade", "websocket");
    request.setRawHeader("Connection", "Upgrade");
    request.setRawHeader("Sec-WebSocket-Version", "13");
    request.setRawHeader("Sec-WebSocket-Key", QUuid::createUuid().toString().remove("{").remove("}").toUtf8().toBase64());

    qDebug() << "Headers:";
    for(const auto& header : request.rawHeaderList()) {
        qDebug() << header << ":" << request.rawHeader(header);
    }

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, wsUrl]() {
        qDebug() << "Response Status Code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Response Headers:";
        for(const auto& header : reply->rawHeaderPairs()) {
            qDebug() << header.first << ":" << header.second;
        }
        QByteArray responseData = reply->readAll();
        qDebug() << "Response Body:" << QString::fromUtf8(responseData);

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 101) {
            m_webSocket.open(wsUrl);
        } else {
            QString errorStr = QString("Handshake failed with status code: %1").arg(
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
            qDebug() << errorStr;
            emit connectionError(errorStr);
        }
        reply->deleteLater();
    });
}

void SpeechClient::stopRecognition()
{
    if (!m_isRecording) {
        return;
    }

    m_isRecording = false;
    if (m_audioSource) {
        m_audioSource->stop();
        if (m_audioDevice)
            m_audioDevice->disconnect();
    }

    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        QJsonObject frame;
        QJsonObject data;
        data["status"] = 2;
        data["audio"] = "";
        data["format"] = "audio/L16;rate=16000";
        data["encoding"] = "raw";
        frame["data"] = data;

        QString frameStr = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        m_webSocket.sendTextMessage(frameStr);

        QTimer::singleShot(500, [this](){
            m_webSocket.close();
        });
    }

    emit statusChanged("停止识别");
}

void SpeechClient::onConnected()
{
    qDebug() << "WebSocket connected successfully!";
    emit statusChanged("已连接，开始录音...");

    QJsonObject startFrame;

    QJsonObject common;
    common["app_id"] = "2c0105db";
    startFrame["common"] = common;

    QJsonObject business;
    business["language"] = "zh_cn";
    business["domain"] = "iat";
    business["accent"] = "mandarin";
    business["vad_eos"] = 3000;
    business["dwa"] = "wpgs";
    startFrame["business"] = business;

    QJsonObject data;
    data["status"] = 0;
    data["format"] = "audio/L16;rate=16000";
    data["encoding"] = "raw";
    data["audio"] = "";
    startFrame["data"] = data;

    QString startFrameStr = QJsonDocument(startFrame).toJson(QJsonDocument::Compact);
    qDebug() << "Sending start frame:" << startFrameStr;
    m_webSocket.sendTextMessage(startFrameStr);

    m_isRecording = true;
    m_audioDevice = m_audioSource->start();
    connect(m_audioDevice, &QIODevice::readyRead, this, &SpeechClient::onAudioDataReady, Qt::UniqueConnection);

}

void SpeechClient::onAudioDataReady()
{
    if (!m_isRecording || m_webSocket.state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray data = m_audioDevice->readAll();
    qDebug() << "Read audio data size:" << data.size() << "bytes";

    if (data.isEmpty()) {
        qDebug() << "Empty audio data received";
        return;
    }

    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / sizeof(qint16);
    qDebug() << "Sample count:" << sampleCount;

    if (sampleCount > 0) {
        qDebug() << "First 5 samples:";
        for (int i = 0; i < qMin(5, sampleCount); ++i) {
            qDebug() << i << ":" << samples[i];
        }
    }

    bool hasSpeech = false;
    qint16 threshold = 1000;

    int speechSamples = 0;
    for (int i = 0; i < sampleCount; ++i) {
        if (abs(samples[i]) > threshold) {
            hasSpeech = true;
            speechSamples++;
        }
    }

    qDebug() << "Speech samples detected:" << speechSamples << "of" << sampleCount;

    if (!hasSpeech) {
        qDebug() << "No speech detected in audio frame";
        return;
    }

    QJsonObject frame;
    QJsonObject dataObj;
    dataObj["status"] = 1;
    dataObj["format"] = "audio/L16;rate=16000";
    dataObj["encoding"] = "raw";
    dataObj["audio"] = QString(data.toBase64());
    frame["data"] = dataObj;

    QString frameStr = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    m_webSocket.sendTextMessage(frameStr);
}

void SpeechClient::onTextMessageReceived(const QString& message)
{
    qDebug() << "完整接收到的消息:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) {
        qDebug() << "Invalid JSON message received";
        return;
    }

    QJsonObject obj = doc.object();

    int code = obj["code"].toInt();
    if (code != 0) {
        QString errorMsg = obj["message"].toString();
        qDebug() << "Error from server: Code=" << code << ", Message=" << errorMsg;

        if (code == 10165) {
            qDebug() << "Session expired, reconnecting...";
            m_webSocket.close();
            QTimer::singleShot(500, this, &SpeechClient::startRecognition);
        }

        emit connectionError(errorMsg);
        return;
    }

    QJsonObject data = obj["data"].toObject();
    if (data.isEmpty()) {
        return;
    }

    QJsonObject result = data["result"].toObject();
    if (result.isEmpty()) {
        return;
    }

    QJsonArray ws = result["ws"].toArray();
    QString text;
    for (const QJsonValue& w : ws) {
        QJsonArray cw = w["cw"].toArray();
        for (const QJsonValue& c : cw) {
            text += c["w"].toString();
        }
    }

    if (!text.isEmpty()) {
        qDebug() << "Recognition result:" << text;
        emit recognitionResult(text);
    }
}

void SpeechClient::handleRecognitionResult(const QJsonObject& result)
{
    QJsonObject data = result["data"].toObject();
    if (data.isEmpty()) {
        return;
    }

    QJsonObject resultObj = data["result"].toObject();
    if (resultObj.isEmpty()) {
        return;
    }

    QJsonArray ws = resultObj["ws"].toArray();
    QString text;
    for (const QJsonValue& w : ws) {
        QJsonArray cw = w["cw"].toArray();
        for (const QJsonValue& c : cw) {
            text += c["w"].toString();
        }
    }

    if (!text.isEmpty()) {
        emit recognitionResult(text);
    }
}

void SpeechClient::handleHandshakeResponse(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Handshake error:" << reply->errorString();
        emit connectionError(reply->errorString());
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "HTTP Status Code:" << statusCode;

    qDebug() << "Response Headers:";
    const auto headers = reply->rawHeaderPairs();
    for(const auto& header : headers) {
        qDebug() << header.first << ":" << header.second;
    }

    if (statusCode == 101) {
        QUrl wsUrl = reply->url();
        m_webSocket.open(wsUrl);
    } else {
        QString errorStr = QString("Handshake failed with status code: %1").arg(statusCode);
        qDebug() << errorStr;
        emit connectionError(errorStr);
    }
}

void SpeechClient::onError(QAbstractSocket::SocketError error)
{
    QString errorStr = QString("WebSocket error: %1 - %2")
    .arg(error)
        .arg(m_webSocket.errorString());
    qDebug() << errorStr;

    qDebug() << "Connection details:";
    qDebug() << "URL:" << m_webSocket.requestUrl().toString();
    qDebug() << "State:" << m_webSocket.state();

    emit connectionError(errorStr);
    stopRecognition();
}

void SpeechClient::onDisconnected()
{
    stopRecognition();
    emit statusChanged("已断开连接");
}

void SpeechClient::onBinaryMessageReceived(const QByteArray& message)
{
    qDebug() << "Received binary message (unexpected):" << message.size() << "bytes";
}
