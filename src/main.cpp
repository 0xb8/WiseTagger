/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "util/misc.h"
#include "util/project_info.h"
#include "window.h"
#include <QApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QSettings>
#include <QTranslator>

namespace logging_category {
	Q_LOGGING_CATEGORY(main, "Main")
}
#define pdbg qCDebug(logging_category::main)
#define pwarn qCWarning(logging_category::main)

int main(int argc, char *argv[])
{
	qSetMessagePattern(QStringLiteral("%{if-warning}[WARN] %{endif}"
					  "%{if-debug}[DBG]  %{endif}"
					  "%{if-info}[INFO] %{endif}"
					  "%{if-critical}[CRIT] %{endif}"
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

	const auto language_code = util::language_from_settings();
	const auto language_name = util::language_name(language_code);
	QLocale::setDefault(language_code);
	const auto qt_qm = QStringLiteral("qt_%1").arg(QLocale(language_code).name());
	const auto wt_qm = QStringLiteral(":/i18n/%1.qm").arg(language_name);

	QTranslator wt, qt;
	if(qt.load(qt_qm, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
	{
		a.installTranslator(&qt);
	} else {
		pwarn << "failed to load" << qt_qm << "from" << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
	}

	if(wt.load(wt_qm)) {
		a.installTranslator(&wt);
	} else {
		pwarn << "failed to load" << wt_qm;
	}

	Window w;
	w.show();

	return a.exec();
}
