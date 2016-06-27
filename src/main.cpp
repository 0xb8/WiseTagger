/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "window.h"
#include "util/misc.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QFile>

int main(int argc, char *argv[])
{
	qSetMessagePattern(QStringLiteral("%{if-warning}[WARN] %{endif}"
					  "%{if-debug}[DBG]  %{endif}"
					  "(%{time h:mm:ss.zzz}) <%{if-category}%{category}%{endif}> "
					  "%{if-debug}%{function}  %{endif}\t%{message}"));
	QApplication a(argc, argv);
	a.setApplicationVersion(QStringLiteral(APP_VERSION));
	a.setApplicationName(QStringLiteral(TARGET_PRODUCT));
	a.setOrganizationName(QStringLiteral(TARGET_COMPANY));
	a.setOrganizationDomain(QStringLiteral("wolfgirl.org"));

	// check if running in portable mode
	if(QFile(qApp->applicationDirPath() +
		 QStringLiteral("/portable.dat")).exists())
	{
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			qApp->applicationDirPath()+QStringLiteral("/settings"));
		QSettings settings;
		settings.setValue(QStringLiteral("settings-portable"), true);
	}

	auto language_code = util::language_from_settings();
	auto language_name = util::language_name(language_code);
	QLocale::setDefault(language_code);

	QTranslator wt, qt;
	if(qt.load(QStringLiteral("qt_%1").arg(QLocale(language_code).name()),
			      QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
		qApp->installTranslator(&qt);
	}

	if(wt.load(QStringLiteral(":/i18n/%1.qm").arg(language_name))) {
		qApp->installTranslator(&wt);
	}

	Window w;
	w.show();

	return a.exec();
}
