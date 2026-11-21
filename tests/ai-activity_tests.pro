QT += core gui quick quickcontrols2 testlib sql network dbus

CONFIG += c++17 testcase
TARGET = ai-activity_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/storage.h \
    ../src/trmclient.h \
    ../src/theme.h \
    ../src/backend.h \
    ../src/parsers/usageparser.h \
    ../src/parsers/pishapedparser.h \
    ../src/parsers/claudeparser.h \
    ../src/parsers/opencodeparser.h \
    ../src/parsers/t3parser.h \
    ../src/parsers/sourceregistry.h

SOURCES += \
    ../src/storage.cpp \
    ../src/trmclient.cpp \
    ../src/theme.cpp \
    ../src/backend.cpp \
    ../src/parsers/pishapedparser.cpp \
    ../src/parsers/claudeparser.cpp \
    ../src/parsers/opencodeparser.cpp \
    ../src/parsers/t3parser.cpp \
    ../src/parsers/sourceregistry.cpp \
    ai-activity_tests.cpp

RESOURCES += ../src/resources.qrc
