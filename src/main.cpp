/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "window.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QFile>

int main(int argc, char *argv[])
{
	qSetMessagePattern(QStringLiteral("%{if-warning}[WARN] %{endif}"
					  "%{if-debug}[DBG]  %{endif}"
					  "(%{time h:mm:ss.zzz}) %{if-category}%{category}: %{endif}"
					  "%{if-debug}%{function}  %{endif}\t%{message}"));
	QApplication a(argc, argv);
	a.setApplicationVersion(QStringLiteral(APP_VERSION));
	a.setApplicationName(QStringLiteral(TARGET_PRODUCT));
	a.setOrganizationName(QStringLiteral(TARGET_COMPANY));
	a.setOrganizationDomain(QStringLiteral("wolfgirl.org"));

	bool portable = false;
	// check if running in portable mode
	if(QFile(qApp->applicationDirPath() +
		 QStringLiteral("/portable.dat")).exists())
	{
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			qApp->applicationDirPath()+QStringLiteral("/settings"));
		portable = true;
	}

	QSettings settings;
	settings.setValue(QStringLiteral("settings-portable"), portable);
	auto l = settings.value(QStringLiteral("window/locale"), QStringLiteral("en")).toString();
	QLocale locale(l);
	QLocale::setDefault(locale);

	QTranslator translator;
	if(translator.load(QString(":/i18n/wisetagger_%1.qm").arg(l)))
		qApp->installTranslator(&translator);

	Window w;
	w.show();

	return a.exec();
}
