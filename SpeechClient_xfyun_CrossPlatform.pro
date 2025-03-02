QT += core gui multimedia widgets websockets network
CONFIG += c++17
TARGET = SpeechClient_xfyun
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    speechclient.cpp

HEADERS += \
    mainwindow.h \
    speechclient.h

CONFIG += lrelease

# macOS 平台特定配置
macx {
    LIBS += -framework Security -framework CoreFoundation
    QMAKE_INFO_PLIST = $${PWD}/Info.plist
}

# 应用程序图标
# win32:RC_ICONS += path/to/icon.ico
# macx:ICON = path/to/icon.icns

target.path = $$[QT_INSTALL_BINS]
!isEmpty(target.path): INSTALLS += target
