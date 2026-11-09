QT += core testlib sql network

CONFIG += c++17 testcase
TARGET = ai-activity_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/storage.h \
    ../src/trmclient.h \
    ../src/theme.h

SOURCES += \
    ../src/storage.cpp \
    ../src/trmclient.cpp \
    ../src/theme.cpp \
    ai-activity_tests.cpp
