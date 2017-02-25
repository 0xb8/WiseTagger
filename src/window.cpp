/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QApplication>
#include <QCollator>
#include <QDebug>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QProxyStyle>
#include <QSettings>
#include <QStyle>
#include <QUrl>
#include <QVersionNumber>
#include <memory>

#include "window.h"
#include "global_enums.h"
#include "util/open_graphical_shell.h"
#include "util/command_placeholders.h"
#include "util/size.h"
#include "util/misc.h"

namespace logging_category {
	Q_LOGGING_CATEGORY(window, "Window")
}
#define pdbg qCDebug(logging_category::window)
#define pwarn qCWarning(logging_category::window)

#define SETT_WINDOW_GEOMETRY    QStringLiteral("window/geometry")
#define SETT_WINDOW_STATE       QStringLiteral("window/state")
#define SETT_LAST_DIR           QStringLiteral("window/last-directory")
#define SETT_LAST_SESSION_DIR   QStringLiteral("window/last-session-directory")

#define SETT_SHOW_MENU          QStringLiteral("window/show-menu")
#define SETT_SHOW_STATUS        QStringLiteral("window/show-statusbar")
#define SETT_SHOW_CURRENT_DIR   QStringLiteral("window/show-current-directory")
#define SETT_SHOW_INPUT         QStringLiteral("window/show-input")
#define SETT_STYLE              QStringLiteral("window/style")
#define SETT_VIEW_MODE          QStringLiteral("window/view-mode")

#define SETT_REPLACE_TAGS       QStringLiteral("imageboard/replace-tags")
#define SETT_RESTORE_TAGS       QStringLiteral("imageboard/restore-tags")

#ifdef Q_OS_WIN
#define SETT_LAST_VER_CHECK     QStringLiteral("last-version-check")
#define SETT_VER_CHECK_ENABLED  QStringLiteral("version-check-enabled")
#define VER_CHECK_URL           QUrl(QStringLiteral("https://bitbucket.org/catgirl/wisetagger/raw/version/current.txt"))
#define VER_CHECK_USERAGENT     QStringLiteral("Mozilla/5.0 (Windows NT 6.1; rv:45.5) Gecko/20100101 Firefox/45.5")
#endif

//------------------------------------------------------------------------------

Window::Window(QWidget *_parent) : QMainWindow(_parent)
	, m_tagger(          this)
	, m_reverse_search(  this)
	, a_open_file(       tr("&Open File..."), nullptr)
	, a_open_dir(        tr("Open &Folder..."), nullptr)
	, a_delete_file(     tr("&Delete Current Image"), nullptr)
	, a_open_post(       tr("Open Imageboard &Post..."), nullptr)
	, a_iqdb_search(     tr("&Reverse Search Image..."), nullptr)
	, a_exit(            tr("&Exit"), nullptr)
	, a_next_file(       tr("&Next Image"), nullptr)
	, a_prev_file(       tr("&Previous Image"), nullptr)
	, a_save_file(       tr("&Save"), nullptr)
	, a_save_next(       tr("Save and Open Next Image"), nullptr)
	, a_save_prev(       tr("Save and Open Previous Image"), nullptr)
	, a_go_to_number(    tr("&Go To File Number..."), nullptr)
	, a_open_session(    tr("Open Session"), nullptr)
	, a_save_session(    tr("Save Session"), nullptr)
	, a_open_loc(        tr("Open &Containing Folder"), nullptr)
	, a_reload_tags(     tr("&Reload Tag File"), nullptr)
	, a_open_tags(       tr("Open Tag File &Location"), nullptr)
	, a_edit_tags(       tr("&Edit Tag File"), nullptr)
	, a_ib_replace(      tr("Re&place Imageboard Tags"), nullptr)
	, a_ib_restore(      tr("Re&store Imageboard Tags"), nullptr)
	, a_show_settings(   tr("P&references..."), nullptr)
	, a_view_minimal(    tr("Mi&nimal View"), nullptr)
	, a_view_statusbar(  tr("&Statusbar"), nullptr)
	, a_view_fullscreen( tr("&Fullscreen"), nullptr)
	, a_view_menu(       tr("&Menu"), nullptr)
	, a_view_input(      tr("Tag &Input"), nullptr)
	, a_view_sort_name(  tr("By File &Name"), nullptr)
	, a_view_sort_type(  tr("By File &Type"), nullptr)
	, a_view_sort_date(  tr("By Modification &Date"), nullptr)
	, a_view_sort_size(  tr("By File &Size"), nullptr)
	, a_about(           tr("&About..."), nullptr)
	, a_about_qt(        tr("About &Qt..."), nullptr)
	, a_help(            tr("&Help..."), nullptr)
	, a_stats(           tr("&Statistics..."), nullptr)
	, ag_sort_criteria(  nullptr)
	, menu_file(         tr("&File"))
	, menu_navigation(   tr("&Navigation"))
	, menu_view(         tr("&View"))
	, menu_sort(         tr("&Sort Queue"))
	, menu_options(      tr("&Options"))
	, menu_commands(     tr("&Commands"))
	, menu_help(	     tr("&Help"))
	, menu_notifications()
	, menu_tray()
	, m_statusbar()
	, m_statusbar_label()
	, m_tray_icon()
{
	setCentralWidget(&m_tagger);
	setAcceptDrops(true);
	updateStyle(); // NOTE: should be called before menus are created.
	createActions();
	createMenus();
	createCommands();
	initSettings();
	QTimer::singleShot(50, this, &Window::parseCommandLineArguments); // NOTE: should be called later to avoid unnecessary media resize after process launch.

#ifdef Q_OS_WIN32
	QTimer::singleShot(1500, this, &Window::checkNewVersion);
#endif

}

//------------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	auto fileName = QFileDialog::getOpenFileName(this,
		tr("Open File"),
		m_last_directory,
		tr("Image Files (%1);;Session Files (%2)")
			.arg(util::join(util::supported_image_formats_namefilter()))
			.arg(FileQueue::sessionNameFilter));
	m_tagger.open(fileName);

}

void Window::directoryOpenDialog()
{
	auto dir = QFileDialog::getExistingDirectory(nullptr,
		tr("Open Directory"),
		m_last_directory,
		QFileDialog::ShowDirsOnly);

	m_tagger.openDir(dir);
}


void Window::updateWindowTitle()
{
	if(m_tagger.isEmpty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	setWindowTitle(tr(Window::MainWindowTitle).arg(
		m_tagger.currentFileName(),
		m_tagger.fileModified() ? QStringLiteral("*") : QStringLiteral(""),
		QString::number(m_tagger.mediaDimensions().width()),
		QString::number(m_tagger.mediaDimensions().height()),
		util::size::printable(m_tagger.mediaFileSize()),
		qApp->applicationVersion()));
}

void Window::updateWindowTitleProgress(int progress)
{
	if(m_tagger.isEmpty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	setWindowTitle(tr(Window::MainWindowTitleProgress).arg(
		m_tagger.currentFileName(),
		m_tagger.fileModified() ? QStringLiteral("*") : QStringLiteral(""),
		QString::number(m_tagger.mediaDimensions().width()),
		QString::number(m_tagger.mediaDimensions().height()),
		util::size::printable(m_tagger.mediaFileSize()),
		qApp->applicationVersion(),
		QString::number(progress)));
}

void Window::updateStatusBarText()
{
	if(m_tagger.isEmpty()) {
		m_statusbar_label.clear();
		return;
	}
	if(m_show_current_directory) {
		statusBar()->showMessage(tr("In directory:  %1")
			.arg(QDir::toNativeSeparators(m_tagger.currentDir())));
	}
	const auto current = m_tagger.queue().currentIndex() + 1u;
	const auto qsize   = m_tagger.queue().size();
	m_statusbar_label.setText(QStringLiteral("%1 / %2  ")
		.arg(QString::number(current), QString::number(qsize)));
}

void Window::updateImageboardPostURL(QString url)
{
	a_open_post.setDisabled(url.isEmpty());
	m_post_url = url;
}

void Window::addNotification(const QString &title, const QString& description, const QString &body)
{
	// remove other notifications of the same type
	removeNotification(title);
	// we want only last notification's action to be triggered
	QObject::disconnect(&m_tray_icon, &QSystemTrayIcon::messageClicked, nullptr, nullptr);

	if(!body.isEmpty()) {
		auto action = menu_notifications.addAction(title);

		connect(action, &QAction::triggered, [title,body,this,action](){
			QMessageBox mb(QMessageBox::Information, title, body, QMessageBox::Ok, this);
			mb.setTextInteractionFlags(Qt::TextBrowserInteraction);
			mb.exec();
			removeNotification(title);
		});
		connect(&m_tray_icon, &QSystemTrayIcon::messageClicked, action, &QAction::trigger);

		menu_notifications.addAction(action);
		++m_notification_count;
	}
	showNotificationsMenu();
	m_tray_icon.setVisible(true);
	m_tray_icon.showMessage(title, description);
}

void Window::removeNotification(const QString& title) {
	const auto actions = menu_notifications.actions();
	for(const auto a : actions) {
		if(a && a->text() == title) {
			menu_notifications.removeAction(a);
			if(m_notification_count > 0) --m_notification_count;
		}
	}
	if(m_notification_count <= 0) {
		hideNotificationsMenu();

	}
	m_tray_icon.setVisible(false);
}

void Window::showNotificationsMenu()
{
	if(m_notification_count > 0) {
		m_notification_display_timer.start(1000);
	}
}

void Window::hideNotificationsMenu()
{
	m_notification_display_timer.stop();
	menu_notifications.setTitle("");
}

void Window::showUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
#ifdef Q_OS_WIN32
	auto progress = m_win_taskbar_button.progress();
	progress->setVisible(true);
	progress->setMaximum(bytesTotal);
	progress->setValue(bytesSent);

	if(bytesSent == bytesTotal) {
		// set indicator to indeterminate mode
		progress->setMaximum(0);
		progress->setValue(0);
	}
#endif
	if(bytesTotal == 0)
		return;

	updateWindowTitleProgress(util::size::percent(bytesSent, bytesTotal));
	statusBar()->showMessage(
		tr("Uploading %1 to iqdb.org...  %2% complete").arg(
			m_tagger.currentFileName(),
			QString::number(util::size::percent(bytesSent, bytesTotal))));
}

void Window::hideUploadProgress()
{
#ifdef Q_OS_WIN32
	m_win_taskbar_button.progress()->setVisible(false);
#endif
	updateWindowTitle();
	statusBar()->showMessage(tr("Done."), 3000);
}

//------------------------------------------------------------------------------
bool Window::eventFilter(QObject*, QEvent *e)
{
	if(e->type() == QEvent::DragEnter) {
		auto drag_event = static_cast<QDragEnterEvent*>(e);

		if(!drag_event->mimeData()->hasUrls())
			return true;

		const auto urls = drag_event->mimeData()->urls();

		if(urls.empty())
			return true;

		if(urls.size() == 1) {
			QFileInfo dropfile(urls.first().toLocalFile());
			if(!(dropfile.isDir()
				|| m_tagger.queue().checkExtension(dropfile)
				|| FileQueue::checkSessionFileSuffix(dropfile)))
			{
				return true;
			}
		}

		drag_event->acceptProposedAction();
		return true;
	}
	if(e->type() == QEvent::Drop) {
		const auto drop_event = static_cast<QDropEvent*>(e);
		const auto fileurls = drop_event->mimeData()->urls();
		QFileInfo dropfile;

		Q_ASSERT(!fileurls.empty());

		if(fileurls.size() == 1) {
			dropfile.setFile(fileurls.first().toLocalFile());
			m_tagger.open(dropfile.absoluteFilePath());
			return true;
		}

		// NOTE: allow user to choose to rename or cancel.
		if(m_tagger.rename() == Tagger::RenameStatus::Cancelled)
			return true;

		QStringList filelist;
		for(const auto& fileurl : fileurls) {
			filelist.push_back(fileurl.toLocalFile());
		}
		m_tagger.queue().assign(filelist);
		m_tagger.openFileInQueue();
		return true;
	}
	return false;
}

void Window::closeEvent(QCloseEvent *e)
{
	if(m_tagger.fileModified()) {
		auto r = m_tagger.rename();
		if(r == Tagger::RenameStatus::Renamed || r == Tagger::RenameStatus::Failed) {
			saveSettings();
			QMainWindow::closeEvent(e);
			return;
		}
		e->ignore();
		return;
	}
	saveSettings();
	QMainWindow::closeEvent(e);
}

void Window::showEvent(QShowEvent *e)
{
#ifdef Q_OS_WIN32
	// NOTE: workaround for taskbar button not working.
	m_win_taskbar_button.setWindow(this->windowHandle());
#endif
	e->accept();
}

//------------------------------------------------------------------------------
void Window::parseCommandLineArguments()
{
	auto args = qApp->arguments();
	args.pop_front();

	if(args.size() == 1) {
		m_tagger.open(args.first());
		return;
	}

	for(const auto& arg : args) {
		if(arg.startsWith(QChar('-'))) {
			continue;
		}

		m_tagger.queue().push(arg);
	}
	m_tagger.queue().sort();
	m_tagger.openFileInQueue();
}

//------------------------------------------------------------------------------
void Window::initSettings()
{
	QSettings sett;

	m_last_directory = sett.value(SETT_LAST_DIR).toString();
	m_view_mode      = sett.value(SETT_VIEW_MODE).value<ViewMode>();
	bool show_status = sett.value(SETT_SHOW_STATUS, false).toBool();
	bool show_menu   = sett.value(SETT_SHOW_MENU,   true).toBool();
	bool show_input  = sett.value(SETT_SHOW_INPUT,  true).toBool();

	bool restored_geo   = restoreGeometry(sett.value(SETT_WINDOW_GEOMETRY).toByteArray());
	bool restored_state = restoreState(sett.value(SETT_WINDOW_STATE).toByteArray());

	if(!restored_state || !restored_geo) {
		resize(1024,640);
		move(100,100);
		showNormal();
	}

	menuBar()->setVisible(show_menu);
	m_tagger.setInputVisible(show_input);
	m_statusbar.setVisible(show_status && m_view_mode != ViewMode::Minimal);

	a_view_fullscreen.setChecked(isFullScreen());
	a_view_minimal.setChecked(m_view_mode == ViewMode::Minimal);
	a_view_statusbar.setChecked(show_status);
	a_view_menu.setChecked(show_menu);
	a_view_input.setChecked(show_input);

	a_ib_replace.setChecked(sett.value(SETT_REPLACE_TAGS, false).toBool());
	a_ib_restore.setChecked(sett.value(SETT_RESTORE_TAGS, true).toBool());
	m_show_current_directory = sett.value(SETT_SHOW_CURRENT_DIR, true).toBool();
}

void Window::saveSettings()
{
	QSettings settings;

	settings.setValue(SETT_WINDOW_GEOMETRY, saveGeometry());
	settings.setValue(SETT_WINDOW_STATE,    saveState());
	// NOTE: remove these after a couple of releases
	settings.remove(QStringLiteral("window/size"));
	settings.remove(QStringLiteral("window/position"));
	settings.remove(QStringLiteral("window/show-fullscreen"));
	settings.remove(QStringLiteral("window/maximized"));
}

void Window::updateSettings()
{
	QSettings settings;
	m_show_current_directory = settings.value(SETT_SHOW_CURRENT_DIR, true).toBool();
	if(m_view_mode == ViewMode::Minimal) {
		m_statusbar.hide();
	} else {
		m_statusbar.setVisible(a_view_statusbar.isChecked());
	}
	updateStyle();
	for(auto action : menu_commands.actions()) {
		this->removeAction(action); // NOTE: to prevent hotkey conflicts
	}
	menu_commands.clear();
	createCommands();
	updateMenus();
}

void Window::updateStyle()
{
	QSettings sett;
	auto style_path = QStringLiteral(":/css/%1.css").arg(sett.value(SETT_STYLE, QStringLiteral("Default")).toString());
	QFile styles_file(style_path);
	bool open = styles_file.open(QIODevice::ReadOnly);
	Q_ASSERT(open);
	qApp->setStyleSheet(styles_file.readAll());
	qApp->setWindowIcon(QIcon(QStringLiteral(":/wisetagger.svg")));
	m_tray_icon.setIcon(this->windowIcon());
}

#ifdef Q_OS_WIN
namespace logging_category {
	Q_LOGGING_CATEGORY(vercheck, "Window.VersionCheck")
}
#define vcwarn qCWarning(logging_category::vercheck)
#define vcdbg  qCDebug(logging_category::vercheck)
void Window::checkNewVersion()
{
	QSettings sett;
	auto last_checked = sett.value(SETT_LAST_VER_CHECK,QDate(2016,1,1)).toDate();
	auto checking_disabled = !sett.value(SETT_VER_CHECK_ENABLED, true).toBool();

	if(checking_disabled || last_checked == QDate::currentDate()) {
		vcdbg << (checking_disabled ? "vercheck disabled" : "vercheck enabled") << last_checked.toString();
		return;
	}
	m_vernam.setProxy(m_reverse_search.proxy());
	connect(&m_vernam, &QNetworkAccessManager::finished, this, &Window::processNewVersion);

	QNetworkRequest req{VER_CHECK_URL};
	req.setHeader(QNetworkRequest::UserAgentHeader, VER_CHECK_USERAGENT);
	m_vernam.get(req);
}

void Window::processNewVersion(QNetworkReply *r)
{
	if(r->error() != QNetworkReply::NoError) {
		vcwarn << "could not access" << r->url() << "with error:" << r->errorString();
		return;
	}

	auto status_code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if(status_code != 200) {
		vcwarn << "wrong HTTP status code:" << status_code;
		return;
	}

	QSettings settings;
	settings.setValue(SETT_LAST_VER_CHECK, QDate::currentDate());

	QString response = r->readAll();
	auto parts = response.split(' ', QString::SkipEmptyParts);
	if(parts.size() < 2) {
		vcwarn << "not enough parts";
		return;
	}

	auto newver = QVersionNumber::fromString(parts[0]);
	if(newver.isNull()) {
		vcwarn << "invalid version";
		return;
	}

	auto url = parts[1].remove('\n');
	bool url_valid = QUrl(url).isValid();
	if(url.isEmpty() || !url_valid) {
		vcwarn << "invalid url";
		return;
	}

	const auto current = QVersionNumber::fromString(qApp->applicationVersion());
	int res = QVersionNumber::compare(current, newver);

	if(res < 0) {
		const auto version_str = newver.toString();
		addNotification(tr("New Version Available"),
			tr("Version %1 is available.").arg(version_str),
			tr("<h3>Updated version available: v%1</h3>"
			   "<p><a href=\"%2\">Click here to download new version</a>.</p>")
				.arg(version_str, url));
	}
}
#undef vcwarn
#undef vcdbg
#endif

void Window::createCommands()
{
	menu_commands.setDisabled(true);
	QSettings settings;
	auto size = settings.beginReadArray(SETT_COMMANDS_KEY);
	for(auto i{0}; i < size; ++i) {
		settings.setArrayIndex(i);
		auto name = settings.value(SETT_COMMAND_NAME).toString();
		auto cmd  = settings.value(SETT_COMMAND_CMD).toStringList();
		auto hkey = settings.value(SETT_COMMAND_HOTKEY).toString();

		if(name == CMD_SEPARATOR) {
			menu_commands.addSeparator();
			continue;
		}

		if(name.isEmpty() || cmd.isEmpty()) {
			continue;
		}

		auto binary = cmd.first();
		if(!QFile::exists(binary)) {
			pwarn << "Invalid command: executable does not exist";
			continue;
		}
		cmd.removeFirst();

		auto action = menu_commands.addAction(name);
		this->addAction(action); // NOTE: to make hotkeys work when menubar is hidden
		action->setIcon(util::get_icon_from_executable(binary));
		action->setEnabled(false);
		if(!hkey.isEmpty()) {
			action->setShortcut(hkey);
		}

		connect(action, &QAction::triggered,
			[this,name,binary,cmd] ()
		{
			auto cmd_tmp = cmd; // NOTE: separate copy is needed instead of mutable lambda
			auto to_native = [](const auto& path)
			{
				return QDir::toNativeSeparators(path);
			};

			auto remove_ext = [](const auto& filename)
			{
				auto lastdot = filename.lastIndexOf('.');
				return filename.left(lastdot);
			};

			for(QString& arg : cmd_tmp) {
				arg.replace(QStringLiteral(CMD_PLDR_PATH),
					to_native(m_tagger.currentFile()));

				arg.replace(QStringLiteral(CMD_PLDR_DIR),
					to_native(m_tagger.currentDir()));

				arg.replace(QStringLiteral(CMD_PLDR_FULLNAME),
					to_native(m_tagger.currentFileName()));

				arg.replace(QStringLiteral(CMD_PLDR_BASENAME),
					to_native(remove_ext(m_tagger.currentFileName())));

			}
			auto success = QProcess::startDetached(binary, cmd_tmp);
			pdbg <<       "QProcess::startDetached(" << binary << "," << cmd_tmp << ") =>" << success;
			if(!success) {
				QMessageBox::critical(this,
					tr("Failed to start command"),
					tr("<p>Failed to launch command <b>%1</b>:</p><p>Could not start <code>%2</code>.</p>")
						.arg(name, binary));
			}
		});
		menu_commands.setEnabled(true);
	}
	settings.endArray();
}

//------------------------------------------------------------------------------
void Window::createActions()
{
	a_open_file.setShortcut(    QKeySequence::Open);
	a_open_dir.setShortcut(     QKeySequence(tr("Ctrl+D", "File|Open Directory")));
	a_next_file.setShortcut(    QKeySequence(Qt::Key_Right));
	a_prev_file.setShortcut(    QKeySequence(Qt::Key_Left));
	a_save_file.setShortcut(    QKeySequence::Save);
	a_save_next.setShortcut(    QKeySequence(Qt::SHIFT + Qt::Key_Right));
	a_save_prev.setShortcut(    QKeySequence(Qt::SHIFT + Qt::Key_Left));
	a_delete_file.setShortcut(  QKeySequence::Delete);
	a_open_post.setShortcut(    QKeySequence(tr("Ctrl+P", "Open post")));
	a_iqdb_search.setShortcut(  QKeySequence(tr("Ctrl+F", "Reverse search")));
	a_open_loc.setShortcut(	    QKeySequence(tr("Ctrl+L", "Open file location")));
	a_help.setShortcut(         QKeySequence::HelpContents);
	a_exit.setShortcut(         QKeySequence::Close);
	a_view_fullscreen.setShortcut(QKeySequence::FullScreen);
	a_view_menu.setShortcut(    QKeySequence(Qt::CTRL + Qt::Key_M));
	a_view_input.setShortcut(   QKeySequence(Qt::CTRL + Qt::Key_I));
	a_go_to_number.setShortcut( QKeySequence(Qt::CTRL + Qt::Key_NumberSign));

	a_next_file.setEnabled(false);
	a_prev_file.setEnabled(false);
	a_save_file.setEnabled(false);
	a_save_next.setEnabled(false);
	a_save_prev.setEnabled(false);
	a_delete_file.setEnabled(false);
	a_open_post.setEnabled(false);
	a_iqdb_search.setEnabled(false);
	a_open_loc.setEnabled(false);
	a_reload_tags.setEnabled(false);
	a_open_tags.setEnabled(false);
	a_edit_tags.setEnabled(false);
	a_save_session.setEnabled(false);
	a_go_to_number.setEnabled(false);

	a_open_post.setStatusTip(  tr("Open imageboard post of this image."));
	a_iqdb_search.setStatusTip(tr("Upload this image to iqdb.org and open search results page in default browser."));
	a_open_loc.setStatusTip(   tr("Open folder where this image is located."));
	a_open_tags.setStatusTip(  tr("Open folder where current tag file is located."));
	a_reload_tags.setStatusTip(tr("Reload changes in tag files and search for newly added tag files."));
	a_edit_tags.setStatusTip(  tr("Open current tag file in default text editor."));
	a_ib_replace.setStatusTip( tr("Toggle replacing certain imageboard tags with their shorter version."));
	a_ib_restore.setStatusTip( tr("Toggle restoring imageboard tags back to their original version."));

	a_ib_replace.setCheckable(true);
	a_ib_restore.setCheckable(true);
	a_view_statusbar.setCheckable(true);
	a_view_fullscreen.setCheckable(true);
	a_view_minimal.setCheckable(true);
	a_view_menu.setCheckable(true);
	a_view_input.setCheckable(true);

	connect(&m_reverse_search, &ReverseSearch::uploadProgress, this, &Window::showUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, &Window::hideUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       &m_tagger.statistics(), &TaggerStatistics::reverseSearched);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, [this]()
	{
		addNotification(tr("IQDB Upload Finished"), tr("Search results page opened in default browser."), QStringLiteral(""));
	});

	connect(&a_open_file,   &QAction::triggered, this, &Window::fileOpenDialog);
	connect(&a_open_dir,    &QAction::triggered, this, &Window::directoryOpenDialog);
	connect(&a_exit,        &QAction::triggered, this, &Window::close);
	connect(&a_about,       &QAction::triggered, this, &Window::about);
	connect(&a_about_qt,    &QAction::triggered, qApp, &QApplication::aboutQt);
	connect(&a_help,        &QAction::triggered, this, &Window::help);
	connect(&a_delete_file, &QAction::triggered, &m_tagger, &Tagger::deleteCurrentFile);
	connect(&a_reload_tags, &QAction::triggered, &m_tagger, &Tagger::reloadTags);
	connect(&a_open_tags,   &QAction::triggered, &m_tagger, &Tagger::openTagFilesInShell);
	connect(&a_edit_tags,   &QAction::triggered, &m_tagger, &Tagger::openTagFilesInEditor);
	connect(&a_stats,       &QAction::triggered, &m_tagger.statistics(), &TaggerStatistics::showStatsDialog);
	connect(&m_tagger,      &Tagger::tagsEdited, this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateMenus);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateStatusBarText);

	connect(&m_tagger,      &Tagger::tagFileChanged, this, [this]()
	{
		addNotification(tr("Tag file changed"),
		                tr("Tag file has been edited or removed.\n"
		                   "All changes successfully applied."),
		                QStringLiteral(""));
	});
	connect(&m_tagger,      &Tagger::fileOpened, [this]()
	{
		updateImageboardPostURL(m_tagger.postURL());
	});
	connect(&a_save_file,   &QAction::triggered, [this]()
	{
		m_tagger.rename();
	});
	connect(&a_next_file,   &QAction::triggered, [this]()
	{
		m_tagger.nextFile();
	});
	connect(&a_prev_file,   &QAction::triggered, [this]()
	{
		m_tagger.prevFile();
	});
	connect(&a_save_next,   &QAction::triggered, [this]()
	{
		m_tagger.nextFile(Tagger::RenameOption::ForceRename);
	});
	connect(&a_save_prev,   &QAction::triggered, [this]()
	{
		m_tagger.prevFile(Tagger::RenameOption::ForceRename);
	});
	connect(&a_open_post,   &QAction::triggered, [this]()
	{
		QDesktopServices::openUrl(m_post_url);
	});
	connect(&a_iqdb_search, &QAction::triggered, [this]()
	{
		m_reverse_search.search(m_tagger.currentFile());
	});
	connect(&a_open_loc,    &QAction::triggered, [this]()
	{
		util::open_file_in_gui_shell(m_tagger.currentFile());
	});
	connect(&a_ib_replace,  &QAction::triggered, [](bool checked)
	{
		QSettings s; s.setValue(SETT_REPLACE_TAGS, checked);
	});
	connect(&a_ib_restore,  &QAction::triggered, [](bool checked)
	{
		QSettings s; s.setValue(SETT_RESTORE_TAGS, checked);
	});
	connect(&a_view_statusbar, &QAction::triggered, [this](bool checked)
	{
		QSettings s; s.setValue(SETT_SHOW_STATUS, checked);
		m_statusbar.setVisible(checked && m_view_mode != ViewMode::Minimal);
	});
	connect(&a_view_fullscreen, &QAction::triggered, [this](bool checked)
	{
		if(checked) {
			m_view_maximized = isMaximized();
			showFullScreen();
		} else {
			if(m_view_maximized) {
				showMaximized();
			} else {
				showNormal();
			}
		}
	});
	connect(&a_view_minimal, &QAction::triggered, [this](bool checked)
	{
		m_view_mode = checked ? ViewMode::Minimal : ViewMode::Normal;
		QSettings s; s.setValue(SETT_VIEW_MODE, QVariant::fromValue(m_view_mode));
		updateSettings();
		m_tagger.updateSettings();
	});
	connect(&a_view_menu, &QAction::triggered, [this](bool checked)
	{
		QSettings s; s.setValue(SETT_SHOW_MENU, checked);
		this->menuBar()->setVisible(checked);
	});
	connect(&a_view_input, &QAction::triggered, [this](bool checked)
	{
		QSettings s; s.setValue(SETT_SHOW_INPUT, checked);
		this->m_tagger.setInputVisible(checked);
	});
	connect(&a_save_session, &QAction::triggered, [this]()
	{
		auto filename = QFileDialog::getSaveFileName(this,
			tr("Save Session"),
			m_last_directory,
			QStringLiteral("Session Files (%1)").arg(FileQueue::sessionNameFilter));
		if(filename.isEmpty())
			return;

		if(!m_tagger.queue().saveToFile(filename)) {
			QMessageBox::critical(this,
				tr("Save Session Error"),
				tr("<p>Could not save session to <b>%1</b>.</p><p>Check file permissions.</p>").arg(filename));
			return;
		}
		QSettings s;
		s.setValue(SETT_LAST_SESSION_DIR, QFileInfo(filename).absolutePath());
	});
	connect(&a_open_session, &QAction::triggered, [this]()
	{
		QSettings s;
		auto lastdir = s.value(SETT_LAST_SESSION_DIR, m_last_directory).toString();
		auto filename = QFileDialog::getOpenFileName(this,
			tr("Open Session"),
			lastdir,
			QStringLiteral("Session Files (%1)").arg(FileQueue::sessionNameFilter));
		m_tagger.openSession(filename);
	});
	connect(&a_go_to_number, &QAction::triggered, [this]()
	{
		auto number = QInputDialog::getInt(this,
			tr("Enter Number"),
			tr("Enter file number to open:"),
			m_tagger.queue().currentIndex()+1, 1, m_tagger.queue().size());
		m_tagger.openFileInQueue(number-1);
	});
	connect(&m_notification_display_timer, &QTimer::timeout, this, [this]()
	{
		static bool which = false;
		auto title = which ? tr("Notifications  ") : tr("Notifications: %1").arg(m_notification_count);
		this->menu_notifications.setTitle(title);
		which = !which;
	});
	connect(&m_tagger, &Tagger::sessionOpened, [](const QString& path)
	{
		QSettings s; s.setValue(SETT_LAST_SESSION_DIR, QFileInfo(path).absolutePath());
	});
	connect(&m_tagger, &Tagger::fileOpened, [this](const QString& path)
	{
		m_last_directory = QFileInfo(path).absolutePath();
		QSettings s; s.setValue(SETT_LAST_DIR, m_last_directory);
	});
	connect(&m_tagger, &Tagger::newTagsAdded, [this](const QStringList& l)
	{
		QString msg = tr("<p>New tags were found, ordered by number of times used:</p>");
		msg.append(QStringLiteral("<ul><li>"));
		for(const auto& e : l) {
			msg.append(e);
			msg.append(QStringLiteral("</li><li>"));
		}
		msg.append(QStringLiteral("</li></ul>"));
		addNotification(tr("New Tags Added"), tr("Check Notifications menu for list of added tags."), msg);
	});
	connect(&a_show_settings, &QAction::triggered, [this]()
	{
		auto sd = new SettingsDialog(this);
		connect(sd, &SettingsDialog::updated, this, &Window::updateSettings);
		connect(sd, &SettingsDialog::updated, &m_reverse_search, &ReverseSearch::updateSettings);
		connect(sd, &SettingsDialog::updated, &m_tagger, &Tagger::updateSettings);
		connect(sd, &SettingsDialog::finished, [sd](int){
			sd->deleteLater();
		});
		sd->open();
	});
	connect(&ag_sort_criteria, &QActionGroup::triggered,[this](QAction* a)
	{
		m_tagger.queue().setSortBy(a->data().value<SortQueueBy>());
		m_tagger.queue().sort();
	});
}


void Window::createMenus()
{
	auto add_action = [this](auto& object,auto& action)
	{
		this->addAction(&action);
		object.addAction(&action);
	};
	auto add_separator = [](auto& object)
	{
		object.addSeparator();
	};

	// File menu actions
	add_action(menu_file, a_open_file);
	add_action(menu_file, a_open_dir);
	add_action(menu_file, a_open_session);
	add_separator(menu_file);
	add_action(menu_file, a_save_file);
	add_action(menu_file, a_save_session);
	add_separator(menu_file);
	add_action(menu_file, a_delete_file);
	add_separator(menu_file);
	add_action(menu_file, a_open_post);
	add_action(menu_file, a_iqdb_search);
	add_separator(menu_file);
	add_action(menu_file, a_exit);

	// Tray context menu actions
	add_action(menu_tray, a_view_fullscreen);
	add_action(menu_tray, a_view_minimal);
	add_separator(menu_tray);
	add_action(menu_tray, a_view_menu);
	add_action(menu_tray, a_view_input);
	add_action(menu_tray, a_view_statusbar);
	add_separator(menu_tray);
	add_action(menu_tray, a_ib_replace);
	add_action(menu_tray, a_ib_restore);
	add_separator(menu_tray);
	add_action(menu_tray, a_show_settings);
	add_action(menu_tray, a_exit);

	// Navigation menu actions
	add_action(menu_navigation, a_next_file);
	add_action(menu_navigation, a_prev_file);
	add_action(menu_navigation, a_go_to_number);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_save_next);
	add_action(menu_navigation, a_save_prev);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_open_loc);
	add_action(menu_navigation, a_open_tags);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_reload_tags);
	add_action(menu_navigation, a_edit_tags);

	// View menu actions
	add_action(menu_view, a_view_fullscreen);
	add_action(menu_view, a_view_minimal);
	add_separator(menu_view);
	add_action(menu_view, a_view_menu);
	add_action(menu_view, a_view_input);
	add_action(menu_view, a_view_statusbar);
	add_separator(menu_view);
	menu_view.addMenu(&menu_sort);

	// Sort menu actions
	add_action(menu_sort, a_view_sort_name);
	add_action(menu_sort, a_view_sort_type);
	add_action(menu_sort, a_view_sort_size);
	add_action(menu_sort, a_view_sort_date);
	a_view_sort_name.setCheckable(true);
	a_view_sort_name.setChecked(true);
	a_view_sort_type.setCheckable(true);
	a_view_sort_size.setCheckable(true);
	a_view_sort_date.setCheckable(true);
	a_view_sort_name.setActionGroup(&ag_sort_criteria);
	a_view_sort_type.setActionGroup(&ag_sort_criteria);
	a_view_sort_size.setActionGroup(&ag_sort_criteria);
	a_view_sort_date.setActionGroup(&ag_sort_criteria);
	a_view_sort_name.setData(QVariant::fromValue(SortQueueBy::FileName));
	a_view_sort_type.setData(QVariant::fromValue(SortQueueBy::FileType));
	a_view_sort_size.setData(QVariant::fromValue(SortQueueBy::FileSize));
	a_view_sort_date.setData(QVariant::fromValue(SortQueueBy::ModificationDate));

	// Options menu actions
	add_action(menu_options, a_ib_replace);
	add_action(menu_options, a_ib_restore);
	add_separator(menu_options);
	add_action(menu_options, a_show_settings);

	// Help menu actions
	add_action(menu_help, a_help);
	add_action(menu_help, a_stats);
	add_separator(menu_help);
	add_action(menu_help, a_about);
	add_action(menu_help, a_about_qt);

	// For enabling addSeparator() in QMenuBar.
	class ProxyStyle : public QProxyStyle
	{
	public:
		virtual int styleHint(StyleHint hint,
		                      const QStyleOption *option = nullptr,
		                      const QWidget *widget = nullptr,
		                      QStyleHintReturn *returnData = nullptr) const override
		{
			if (hint == SH_DrawMenuBarSeparator)
				return hint; // NOTE: seems like returning any non-zero value works.

			return QProxyStyle::styleHint(hint, option, widget, returnData);
		}
	};
	auto proxy_style = new ProxyStyle();
	proxy_style->setParent(this);
	menuBar()->setStyle(proxy_style);

	// Menu bar menus
	menuBar()->addMenu(&menu_file);
	menuBar()->addMenu(&menu_navigation);
	menuBar()->addMenu(&menu_view);
	menuBar()->addMenu(&menu_commands);
	menuBar()->addMenu(&menu_options);
	menuBar()->addMenu(&menu_help);
	menuBar()->addSeparator();
	menuBar()->addMenu(&menu_notifications);

	// Tray context menu
	m_tray_icon.setContextMenu(&menu_tray);

	// Status bar items
	setStatusBar(&m_statusbar);
	m_statusbar.setObjectName(QStringLiteral("StatusBar"));
	m_statusbar_label.setObjectName(QStringLiteral("StatusbarInfo"));
	m_statusbar.setSizeGripEnabled(false);
	m_statusbar.addPermanentWidget(&m_statusbar_label);
}

void Window::updateMenus()
{
	bool val = m_tagger.isEmpty();
	a_next_file.setDisabled(val);
	a_prev_file.setDisabled(val);
	a_save_file.setDisabled(val);
	a_save_next.setDisabled(val);
	a_save_prev.setDisabled(val);
	a_delete_file.setDisabled(val);
	a_reload_tags.setDisabled(val);
	a_open_tags.setDisabled(val);
	a_edit_tags.setDisabled(val);
	a_open_post.setDisabled(m_post_url.isEmpty());
	a_iqdb_search.setDisabled(val);
	a_open_loc.setDisabled(val);
	a_save_session.setDisabled(val);
	a_go_to_number.setDisabled(val);
	for(auto action : menu_commands.actions()) {
		action->setDisabled(val);
	}
}

//------------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(nullptr,
	tr("About %1").arg(QStringLiteral(TARGET_PRODUCT)),
	util::read_resource_html("about.html").arg(
		QStringLiteral(TARGET_PRODUCT),
		qApp->applicationVersion(),
		QStringLiteral(BUILD_FROM)).arg(QDate::currentDate().year()));
}

void Window::help()
{
	QSettings settings;
	bool portable = settings.value(QStringLiteral("settings-portable"), false).toBool();

	QMessageBox::information(nullptr,
	tr("Help"),
	util::read_resource_html("help.html").arg(
		a_ib_replace.text().remove('&'),
		a_ib_restore.text().remove('&'),
		portable ? tr("enabled", "portable") : tr("disabled", "portable"),
		QStringLiteral(TARGET_PRODUCT)));
}

//------------------------------------------------------------------------
