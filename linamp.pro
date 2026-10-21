QT += quick multimedia
CONFIG += c++20 link_pkgconfig release
CONFIG -= app_bundle
PKGCONFIG += libavformat libavcodec libavutil libswresample

TEMPLATE = app
TARGET = linamp
RESOURCES += resources.qrc

DEFINES += APP_QML_DIR=\\\"$$PWD/qml\\\"

SOURCES += \
    src/main.cpp \
    src/audioengine.cpp \
    src/shell.cpp \
    src/prober.cpp

HEADERS += \
    src/audioengine.h \
    src/shell.h \
    src/prober.h

INCLUDEPATH += src

QMAKE_CXXFLAGS += -Wall -Wextra
