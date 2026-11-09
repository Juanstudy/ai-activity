QT += core gui quick quickcontrols2 testlib sql network dbus

CONFIG += c++17 testcase
TARGET = ai-activity_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/storage.h \
    ../src/trmclient.h \
    ../src/theme.h \
    ../src/backend.h

SOURCES += \
    ../src/storage.cpp \
    ../src/trmclient.cpp \
    ../src/theme.cpp \
    ../src/backend.cpp \
    ai-activity_tests.cpp

RESOURCES += ../src/resources.qrc
