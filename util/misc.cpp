/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QSettings>
#include <QFile>
#include <QDir>
#include <QApplication>
#include "misc.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <QtWin>
#endif

#define SETT_LOCALE     QStringLiteral("window/language")
#define SETT_LOCALE_OLD QStringLiteral("window/locale")
#define HTML_LOCATION   QStringLiteral(":/html/%1/%2")

QString util::read_resource_html(const char *filename)
{
	auto language = QLocale::languageToString(language_from_settings());

	QFile file(HTML_LOCATION.arg(language, filename));
	bool open = file.open(QIODevice::ReadOnly);
	if(!open) {
		file.setFileName(HTML_LOCATION.arg(QStringLiteral("English"), filename));
		open = file.open(QIODevice::ReadOnly);
	}
	Q_ASSERT(open && "resource file not opened");
	return QString(file.readAll());
}

QString util::duration(uint64_t secs)
{
	auto yr = secs / 31536000;
	auto mon = (secs / 2678400) % 12;
	auto d = (secs / 86400) % 31;
	auto hr = (secs / 3600) % 24;
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

QStringList util::parse_arguments(const QString & args)
{
	QStringList ret;
	QString curr_arg;

	bool open_quote = false;
	for(auto c : args) {
		if(c == '"') {
			open_quote = !open_quote;
			continue;
		}
		if(c.isSpace() && !open_quote) {
			if(!curr_arg.isEmpty()) {
				ret.push_back(curr_arg);
				curr_arg.clear();
			}
			continue;
		}
		curr_arg.push_back(c);
	}
	if(!curr_arg.isEmpty()) {
		ret.push_back(curr_arg);
	}
	return ret;
}


QIcon util::get_icon_from_executable(const QString &path)
{
#ifdef Q_OS_WIN
	auto widepath = QDir::toNativeSeparators(path).toStdWString();
	auto hinstance = ::GetModuleHandle(NULL);
	auto hicon = ::ExtractIcon(hinstance, widepath.c_str(), 0);
	auto ricon = QIcon();
	if(reinterpret_cast<uintptr_t>(hicon) > 1) { // NOTE: winapi derp
		ricon.addPixmap(QtWin::fromHICON(hicon));
	}
	::DestroyIcon(hicon);
	return ricon;
#else
	return QIcon(); // TODO: load icon from .desktop file on linux
#endif
}

QString util::language_name(QLocale::Language lang)
{
	return QLocale::languageToString(lang);
}

QLocale::Language util::language_code(const QString& name)
{
	auto lang_meta = QMetaEnum::fromType<QLocale::Language>();
	bool ok = false;
	auto ret = lang_meta.keyToValue(qPrintable(name), &ok);
	if(ok) {
		return static_cast<QLocale::Language>(ret);
	}
	return QLocale::English;
}

QLocale::Language util::language_from_settings()
{
	QSettings settings;
	auto code = settings.value(SETT_LOCALE);
	if(!code.isValid()) {
		code = QLocale::English;
		settings.remove(SETT_LOCALE_OLD);
		settings.setValue(SETT_LOCALE, code);
	}
	return static_cast<QLocale::Language>(code.toInt());
}


