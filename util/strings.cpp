/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */


#include <QCoreApplication>
#include "util/strings.h"


QString util::duration(uint64_t duration_sec, bool with_seconds)
{
	auto yr  = duration_sec / 31536000;
	auto mon = (duration_sec / 2678400) % 12;
	auto d   = (duration_sec / 86400) % 31;
	auto hr  = (duration_sec / 3600) % 24;
	auto min = (duration_sec / 60) % 60;
	auto s   = (with_seconds || duration_sec < 60) ? duration_sec % 60 : 0;

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

QStringList util::split_unquoted(const QString& str, QChar separator)
{
	QStringList res;
	bool quote_found = false;
	int part_begin = 0;

	for (int i = 0; i < str.size(); ++i) {
		if (str[i] == QChar('"'))
			quote_found = !quote_found;

		bool is_sep = str[i] == separator;
		bool at_end = i == str.size() - 1;

		if ((is_sep && !quote_found) || at_end) {
			auto s = str.mid(part_begin, i - part_begin + at_end).trimmed();
			if(!s.isEmpty())
				res.push_back(s);
			part_begin = i;
		}
	}
	return res;
}
