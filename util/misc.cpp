/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QSettings>
#include <QFile>
#include <QApplication>
#include "misc.h"

#define SETT_LOCALE	QStringLiteral("window/locale")
#define HTML_LOCATION	QStringLiteral(":/html/%1/%2")

QByteArray util::read_resource_html(const char *filename)
{
	QSettings settings;
	auto locale = settings.value(SETT_LOCALE, QStringLiteral("en")).toString();

	QFile file(HTML_LOCATION.arg(locale, filename));
	bool open = file.open(QIODevice::ReadOnly);
	if(!open) {
		file.setFileName(HTML_LOCATION.arg(QStringLiteral("en")).arg(filename));
		open = file.open(QIODevice::ReadOnly);
	}
	Q_ASSERT(open && "resource file not opened");
	return file.readAll();
}

QString util::duration(uint64_t secs)
{
	auto yr = secs / 31536000;
	auto mon = (secs / 2678400) % 12;
	auto d = (secs / 86400) % 31;
	auto hr = (secs / 3600) % 60;
	auto min = (secs / 60) % 60;

	auto s = secs % 60;
	auto sec  =            qApp->translate("Duration", "%n seconds", "", s);
	auto mins = min != 0 ? qApp->translate("Duration", "%n minutes", "", min) : QStringLiteral("");
	auto hrs  = hr  != 0 ? qApp->translate("Duration", "%n hours", "", hr)    : QStringLiteral("");
	auto days = d   != 0 ? qApp->translate("Duration", "%n days", "", d)      : QStringLiteral("");
	auto mons = mon != 0 ? qApp->translate("Duration", "%n months", "", mon)  : QStringLiteral("");
	auto yrs  = yr  != 0 ? qApp->translate("Duration", "%n years", "", yr)    : QStringLiteral("");

	return QStringLiteral("%1 %2 %3 %4 %5 %6").arg(yrs, mons, days, hrs, mins, sec);
}
