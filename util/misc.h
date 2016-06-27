/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QByteArray>
#include <QStringList>
#include <QIcon>
#include <QLocale>

namespace util {

auto read_resource_html(const char* filename) -> QString;

auto language_from_settings() -> QLocale::Language;

auto language_code(const QString& name) -> QLocale::Language;

auto language_name(QLocale::Language) -> QString;

auto duration(std::uint64_t ms) -> QString;

auto parse_arguments(const QString& args) -> QStringList;

auto get_icon_from_executable(const QString& path) -> QIcon;

}
