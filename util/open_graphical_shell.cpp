#include "open_graphical_shell.h"

#ifdef Q_OS_WIN32
#include <QProcess>
#include <QStringList>
#include <QMessageBox>
#include <QDir>
#else
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#endif


void open_file_in_graphical_shell(const QString &file) {
	if(file.isEmpty())
		return;

#ifdef Q_OS_WIN32
	QStringList params;
	params += "/select,";
	params += QDir::toNativeSeparators(file);
	qint64 pid = 0ll;
	QProcess::startDetached("explorer.exe", params, QString(), &pid);

	if(pid == 0ll) {
		QMessageBox::critical(nullptr,
			"Error starting process",
			"<p>Could not start exploler.exe</p>"\
			"<p>Make sure it exists (<b>HOW U EVEN ШINDOШS, BRO?</b>) and <b>PATH</b> environment variable is correct.");
	}

#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).path()));
#endif
}
