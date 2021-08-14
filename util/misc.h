/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

/** @dir util
 * @brief Misc utilities
 */

/**
 * @file misc.h
 * @brief Miscellaneous utilities
 */

#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <QByteArray>
#include <QStringList>
#include <QIcon>
#include <QLocale>
#include <type_traits>
#include <stdint.h>

/**
 * @namespace util
 * @brief Miscellaneous utilities
 */
namespace util {

/// Read resource HTML file contents
QString                 read_resource_html(const char* filename);

/// language saved in settings
QLocale::Language       language_from_settings();

/// Language code corresponding to language \p name string
QLocale::Language       language_code(const QString& name);

/// Language name string from language \p code
QString                 language_name(QLocale::Language code);

/// String with human-readable time duration. E.g. "1 year 3 months 12 days 3 hours 8 minutes 1 second"
QString                 duration(uint64_t seconds, bool with_minutes=true, bool with_seconds=true);

/// Load icon from executable file (windows-only)
QIcon                   get_icon_from_executable(const QString& path);

/// All supported image formats filter list
QStringList             supported_image_formats_namefilter();

/// All supported video formats filter list
QStringList             supported_video_formats_namefilter();

/// Guessed image format for \p filename
QByteArray              guess_image_format(const QString& filename);

/// Join list of strings with \p separator
QString                 join(const QStringList&, QChar separator = QChar(' '));

/// Split string by \p separator. Empty parts are skipped.
QStringList             split(const QString& str, QChar separator = QChar(' '));

/// Tests whether the string contains only hexadecimal digits
bool                    is_hex_string(const QString& str);

/// Replace reserved special characters in a string.
void                    replace_special(QString& str);

/// Save current settings into \p path file
bool                    backup_settings_to_file(const QString& path);

/// Load settings from \p path
bool                    restore_settings_from_file(const QString& path);

} // namespace util

#endif // UTIL_MISC_H
