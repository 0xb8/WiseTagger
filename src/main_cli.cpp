/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "util/project_info.h"
#include "tagger_commandline.h"
#include <QCoreApplication>



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


	QCoreApplication a(argc, argv);
	a.setApplicationVersion(QStringLiteral(APP_VERSION));
	a.setApplicationName(QStringLiteral(TARGET_PRODUCT));
	a.setOrganizationName(QStringLiteral(TARGET_COMPANY));
	a.setOrganizationDomain(QStringLiteral("wolfgirl.org"));

	TaggerCommandLineInterface cli;
	return cli.parseArguments(a.arguments());
}
