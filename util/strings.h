/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

/** @dir util
 * @brief Misc utilities
 */

/**
 * @file strings.h
 * @brief String utilities
 */

#ifndef UTIL_STRINGS_H
#define UTIL_STRINGS_H

#include <QString>
#include <QStringList>


namespace util {

	/// String with human-readable time duration. E.g. "1 year 3 months 12 days 3 hours 8 minutes 1 second"
	QString     duration(uint64_t seconds, bool with_minutes=true, bool with_seconds=true);

	/// Join list of strings with \p separator
	QString     join(const QStringList&, QChar separator = QChar(' '));

	/// Split string by \p separator. Empty parts are skipped.
	QStringList split(const QString& str, QChar separator = QChar(' '));

	/// Tests whether the string contains only hexadecimal digits
	bool        is_hex_string(const QString& str);

	/// Replace reserved special characters in a string.
	void        replace_special(QString& str);

}

#endif // UTIL_STRINGS_H
