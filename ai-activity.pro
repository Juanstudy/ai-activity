QT += core gui qml quick quickcontrols2 sql network dbus

CONFIG += c++17 release
TARGET = ai-activity
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/storage.h \
    src/trmclient.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/storage.cpp \
    src/trmclient.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
