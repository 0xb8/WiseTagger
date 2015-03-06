/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "window.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setApplicationVersion(APP_VERSION);
	a.setApplicationName(TARGET_PRODUCT);
	a.setOrganizationName(TARGET_COMPANY);
	a.setOrganizationDomain("wolfgirl.org");
	Window w;
	w.show();

	return a.exec();
}
