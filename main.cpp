#include "mainwindow.h"

#include <QApplication>

#include <QSslConfiguration>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    MainWindow w;
    w.show();
    return a.exec();
}
