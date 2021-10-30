# Copyright © 2014 cat <cat@wolfgirl.org>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
#
#-------------------------------------------------
#
# Project created by QtCreator 2014-01-14 17:50:00
#
#-------------------------------------------------

QT += core gui network widgets svg multimedia multimediawidgets

win32 {
    QT += winextras
    RC_ICONS += resources/icon.ico
}

TARGET = WiseTagger

QMAKE_TARGET_COMPANY = catgirl
QMAKE_TARGET_PRODUCT = WiseTagger
QMAKE_TARGET_DESCRIPTION = WiseTagger: Simple picture tagging tool
QMAKE_TARGET_COPYRIGHT = cat@wolfgirl.org

VERSION = "0.6.1"
Release:DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT

DEFINES +=                                           \
    QT_DEPRECATED_WARNINGS                           \
    APP_VERSION=\\\"$$VERSION\\\"                    \
    TARGET_PRODUCT=\\\"$$QMAKE_TARGET_PRODUCT\\\"    \
    TARGET_COMPANY=\\\"$$QMAKE_TARGET_COMPANY\\\"

CONFIG += c++14

Debug:PRECOMPILED_HEADER += util/precompiled.h

SOURCES +=                                           \
    src/file_queue.cpp                               \
    src/filter_dialog.cpp                            \
    src/global_enums.cpp                             \
    src/input.cpp                                    \
    src/main.cpp                                     \
    src/multicompleter.cpp                           \
    src/picture.cpp                                  \
    src/reverse_search.cpp                           \
    src/settings_dialog.cpp                          \
    src/statistics.cpp                               \
    src/tagger.cpp                                   \
    src/tag_parser.cpp                               \
    src/window.cpp                                   \
    util/imagecache.cpp                              \
    util/misc.cpp                                    \
    util/open_graphical_shell.cpp                    \
    util/tag_fetcher.cpp

HEADERS  +=                                          \
    src/file_queue.h                                 \
    src/filter_dialog.h                              \
    src/global_enums.h                               \
    src/input.h                                      \
    src/multicompleter.h                             \
    src/picture.h                                    \
    src/reverse_search.h                             \
    src/settings_dialog.h                            \
    src/statistics.h                                 \
    src/tagger.h                                     \
    src/tag_parser.h                                 \
    src/window.h                                     \
    util/command_placeholders.h                      \
    util/imageboard.h                                \
    util/imagecache.h                                \
    util/misc.h                                      \
    util/network.h                                   \
    util/open_graphical_shell.h                      \
    util/project_info.h                              \
    util/size.h                                      \
    util/tag_fetcher.h                               \
    util/traits.h                                    \
    util/unordered_map_qt.h

RESOURCES +=                                         \
    resources/resources.qrc

FORMS +=                                             \
    ui/settings.ui

TRANSLATIONS = resources/i18n/Russian.ts             \
	       resources/i18n/English.ts

isEmpty(PREFIX) {
    PREFIX = /usr/local/
}

target.path = $$PREFIX/bin
INSTALLS += target
