/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <QByteArray>
#include <QStringList>
#include <QIcon>
#include <QLocale>
#include <type_traits>

namespace util {

QString                 read_resource_html(const char* filename);
QLocale::Language       language_from_settings();
QLocale::Language       language_code(const QString& name);
QString                 language_name(QLocale::Language);
QString                 duration(std::uint64_t ms);
QIcon                   get_icon_from_executable(const QString& path);
QStringList             supported_image_formats_namefilter();
QByteArray              guess_image_format(const QString& filename);
QString                 join(const QStringList&, QChar separator = QChar(' '));
bool                    backup_settings_to_file(const QString& path);
bool                    restore_settings_from_file(const QString& path);

} // namespace util

#endif // UTIL_MISC_H
