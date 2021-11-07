/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */


#include <QCoreApplication>
#include "util/strings.h"


QString util::duration(uint64_t seconds, bool with_minutes, bool with_seconds)
{
	auto yr = seconds / 31536000;
	auto mon = (seconds / 2678400) % 12;
	auto d = (seconds / 86400) % 31;
	auto hr = (seconds / 3600) % 24;
	auto min = with_minutes ? (seconds / 60) % 60 : 0;
	auto s = with_seconds ? seconds % 60 : 0;

	auto sec  = s   != 0 ? qApp->translate("Duration", "%n seconds", "", s)   : QStringLiteral("");
	auto mins = min != 0 ? qApp->translate("Duration", "%n minutes", "", min) : QStringLiteral("");
	auto hrs  = hr  != 0 ? qApp->translate("Duration", "%n hours", "", hr)    : QStringLiteral("");
	auto days = d   != 0 ? qApp->translate("Duration", "%n days", "", d)      : QStringLiteral("");
	auto mons = mon != 0 ? qApp->translate("Duration", "%n months", "", mon)  : QStringLiteral("");
	auto yrs  = yr  != 0 ? qApp->translate("Duration", "%n years", "", yr)    : QStringLiteral("");

	auto res = QStringLiteral("%1 %2 %3 %4 %5 %6").arg(yrs, mons, days, hrs, mins, sec);
	return res.simplified();
}


QString util::join(const QStringList& list, QChar separator)
{
	return list.join(separator);
}


void util::replace_special(QString & str)
{
	for(auto& c : str) switch (c.unicode())
	{
		case '"':
		case '\'':
		case ':':
		case '*':
		case '<':
		case '>':
		case '?':
		case '|':
		case '/':
		case '\\':
			c = '_';
	}
}

bool util::is_hex_string(const QString & str)
{
	const char valid_chars[] = "1234567890abcdef";
	for (auto c : str) {
		// ignore \0 terminator
		const auto end = std::prev(std::end(valid_chars));
		auto pos = std::find(std::begin(valid_chars), end, c.toLower().toLatin1());
		if (pos == end)
			return false;
	}
	return true;
}

QStringList util::split(const QString& str, QChar separator)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	auto flags = Qt::SkipEmptyParts;
#else
	auto flags = QString::SkipEmptyParts;
#endif
	return str.split(separator, flags);
}
