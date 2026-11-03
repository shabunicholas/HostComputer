QT += core serialport
QT -= gui

CONFIG += console
CONFIG -= app_bundle

# Qt 6 要求 C++17；Qt 5 用 C++11 即可
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

TEMPLATE = app
TARGET = ModbusSlaveTool

# 复用而不是复制：
#   ../        = tool/           → modbusslave.h / .cpp
#   ../..      = 仓库根目录       → modbusprotocol.h / .cpp
INCLUDEPATH += .. ../..

SOURCES += \
    main_slavetool.cpp \
    ../../modbusprotocol.cpp \
    ../modbusslave.cpp

HEADERS += \
    ../../modbusprotocol.h \
    ../modbusslave.h

# 生成到上层 build 之外的独立目录，避免和自测工程互相覆盖
DESTDIR = $$PWD/build
