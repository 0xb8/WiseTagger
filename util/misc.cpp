/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QSettings>
#include <QFile>
#include "misc.h"

#define SETT_LOCALE QStringLiteral("window/locale")

QByteArray util::read_resource_html(const char *filename)
{
	QSettings settings;
	auto locale = settings.value(SETT_LOCALE).toString();

	QFile file(QString(":/html/%1/%2").arg(locale, filename));
	file.open(QIODevice::ReadOnly);
	return file.readAll();
}
