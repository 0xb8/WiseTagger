#-------------------------------------------------
#
# Project created by QtCreator 2014-01-14T17:50:00
#
#-------------------------------------------------

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = WiseTagger

QMAKE_TARGET_COMPANY = Dobrobox (dobrobox.org)
QMAKE_TARGET_PRODUCT = WiseTagger
QMAKE_TARGET_DESCRIPTION = Simple picture tagger
QMAKE_TARGET_COPYRIGHT = cat@wolfgirl.org

VERSION = 0.4
Release:DEFINES += QT_NO_DEBUG_OUTPUT
Debug:DEFINES += NO_PARSER_DEBUG

DEFINES += APP_VERSION=\\\"$$VERSION\\\" TARGET_PRODUCT=\\\"$$QMAKE_TARGET_PRODUCT\\\"
CONFIG += c++11


SOURCES +=\
    picture.cpp \
    window.cpp \
    main.cpp \
    tagger.cpp \
    multicompleter.cpp \
    input.cpp

HEADERS  += \
    picture.h \
    window.h \
    tagger.h \
    multicompleter.h \
    input.h \
    unordered_map_qt.h

RESOURCES += \
    resources.qrc
