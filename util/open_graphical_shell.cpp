/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "open_graphical_shell.h"
#include <QFile>

#ifdef Q_OS_WIN32
#include <QProcess>
#include <QStringList>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#else
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#endif


void util::open_file_in_gui_shell(const QString &file) {
	if(!QFile::exists(file))
		return;

#ifdef Q_OS_WIN32
	QStringList params;
	params += "/select,";
	params += QDir::toNativeSeparators(file);
	qint64 pid = 0ll;
	QProcess::startDetached("explorer.exe", params, QString(), &pid);

	if(pid == 0ll) {
		QMessageBox::critical(nullptr,
			qApp->translate("OpenInGuiShell","Error starting process"),
			qApp->translate("OpenInGuiShell","<p>Could not start Explorer</p>"\
			"<p>Make sure system <b>PATH</b> environment variable is correct."));
	}

#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).path()));
#endif
}
