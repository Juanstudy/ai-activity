QT += core gui qml quick quickcontrols2 sql network dbus

CONFIG += c++17 release
TARGET = ai-activity
TEMPLATE = app

INCLUDEPATH += src

HEADERS += \
    src/backend.h \
    src/storage.h \
    src/trmclient.h \
    src/theme.h \
    src/translator.h \
    src/parsers/usageparser.h \
    src/parsers/pishapedparser.h \
    src/parsers/claudeparser.h \
    src/parsers/sourceregistry.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/storage.cpp \
    src/trmclient.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/parsers/pishapedparser.cpp \
    src/parsers/claudeparser.cpp \
    src/parsers/sourceregistry.cpp

RESOURCES += src/resources.qrc
