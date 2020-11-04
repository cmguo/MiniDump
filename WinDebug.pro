TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        fontdump.cpp \
        main.cpp \
        minidumpper.cpp

HEADERS += \
    fontdump.h \
    minidumpper.h

LIBS += -lAdvapi32 -lUser32 -lShell32 -lGdi32
