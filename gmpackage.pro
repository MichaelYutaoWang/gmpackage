#-------------------------------------------------
#
# Project created by QtCreator 2016-12-28T17:03:31
#
#-------------------------------------------------

QT       += core gui

TARGET = gmpackage
CONFIG   += console
#CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    gmpackagebuilder.cpp \
    gmpackageinstaller.cpp \
    gmpackagemanager.cpp \
    encrypt_rc4.cpp

HEADERS += \
    gmpackagebuilder.h \
    gmpackageinstaller.h \
    gmpackagemanager.h
