#include "window.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setApplicationVersion(APP_VERSION);
	a.setApplicationName(TARGET_PRODUCT);
	Window w;
	w.show();

	return a.exec();
}
