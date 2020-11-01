TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
        minidumpper.cpp

HEADERS += \
    minidumpper.h

LIBS += -lAdvapi32 -lUser32 -lShell32
