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
#include <QStyleFactory>

#define SETT_STYLE              QStringLiteral("window/style")


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
					  "(%{time h:mm:ss.zzz}) "
	                                  "%{if-category}<%{category}> %{endif}"
					  "%{if-debug}%{function} %{threadid} %{endif}"
	                                  "    %{message}"));

	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#if defined (Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	qputenv("QT_QPA_PLATFORM", "windows:darkmode=1");
#endif

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

	QSettings sett;
	auto theme_name = sett.value(SETT_STYLE, QStringLiteral("Default")).toString();
	if (theme_name == QStringLiteral("Dark")) {
		auto fusion = QStyleFactory::create("fusion");
		QApplication::setStyle(fusion);

		QPalette palette;
		palette.setColor(QPalette::Window, QColor(35, 35, 35));
		palette.setColor(QPalette::WindowText, QColor(238, 238, 238));
		palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(102, 102, 102));
		palette.setColor(QPalette::Base, QColor(24, 24, 24));
		palette.setColor(QPalette::AlternateBase, QColor(40, 40, 40));
		palette.setColor(QPalette::ToolTipBase, Qt::white);
		palette.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
		palette.setColor(QPalette::Text, QColor(220, 220, 220));
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
		palette.setColor(QPalette::Dark, QColor(35, 35, 35));
		palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
		palette.setColor(QPalette::Button, QColor(40, 40, 40));
		palette.setColor(QPalette::Disabled, QPalette::Button, QColor(35, 35, 35));
		palette.setColor(QPalette::ButtonText, QColor(240, 240, 240));
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
		palette.setColor(QPalette::BrightText, Qt::red);
		palette.setColor(QPalette::Link, QColor(0, 137, 204));

		palette.setColor(QPalette::Active,   QPalette::Highlight, QColor(42, 130, 218));
		palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(80, 80, 80));
		palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 60));

		palette.setColor(QPalette::Active,   QPalette::HighlightedText, QColor(255, 255, 255));
		palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(240, 240, 240));
		palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
		QApplication::setPalette(palette);
	}

	auto style_path = QStringLiteral(":/css/%1.css").arg(theme_name);
	QFile styles_file(style_path);
	bool open = styles_file.open(QIODevice::ReadOnly);
	Q_ASSERT(open);
	a.setStyleSheet(styles_file.readAll());

	Window w;
	w.show();

	return a.exec();
}
