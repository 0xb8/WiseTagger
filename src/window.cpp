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
#include <QNetworkProxyFactory>
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
#include "util/strings.h"
#include "util/misc.h"
#include "util/network.h"
#include "statistics.h"

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
#define SETT_SHOW_STATUS_MAIN   QStringLiteral("window/show-status-in-main")
#define SETT_SHOW_INPUT         QStringLiteral("window/show-input")
#define SETT_STYLE              QStringLiteral("window/style")
#define SETT_VIEW_MODE          QStringLiteral("window/view-mode")
#define SETT_EDIT_MODE          QStringLiteral("window/edit-mode")
#define SETT_FIT_TO_SCREEN      QStringLiteral("window/fit-to-screen")
#define SETT_NAVIGATE_BY_WHEEL  QStringLiteral("window/scroll-navigation")

#define SETT_PLAY_MUTE          QStringLiteral("window/video_mute")

#define SETT_REPLACE_TAGS       QStringLiteral("imageboard/replace-tags")
#define SETT_RESTORE_TAGS       QStringLiteral("imageboard/restore-tags")
#define SETT_FORCE_AUTHOR_FIRST	QStringLiteral("imageboard/force-author-first")

#ifdef Q_OS_WIN
#define SETT_LAST_VER_CHECK     QStringLiteral("last-version-check")
#define SETT_VER_CHECK_ENABLED  QStringLiteral("version-check-enabled")
#define VER_CHECK_URL           QUrl(QStringLiteral("https://raw.githubusercontent.com/0xb8/WiseTagger/version/current.txt"))

#endif

//------------------------------------------------------------------------------

Window::Window(QWidget *_parent) : QMainWindow(_parent)
	, m_tagger(          this)
	, m_reverse_search(  this)
	, a_open_file(       tr("&Open File..."), nullptr)
	, a_open_dir(        tr("Open &Folder..."), nullptr)
	, a_open_dir_recurse(tr("Open Folder Recursi&vely..."), nullptr)
	, a_delete_file(     tr("&Delete Current Image"), nullptr)
	, a_copy_file(       tr("Copy image to clipboard"), nullptr)
	, a_open_post(       tr("Open Imageboard &Post..."), nullptr)
	, a_iqdb_search(     tr("&Reverse Search Image..."), nullptr)
	, a_exit(            tr("&Exit"), nullptr)
	, a_hide(            tr("Hide to tray"), nullptr)
	, a_set_queue_filter(tr("Set &Queue Filter..."), nullptr)
	, a_next_file(       tr("&Next Image"), nullptr)
	, a_prev_file(       tr("&Previous Image"), nullptr)
	, a_save_file(       tr("&Save"), nullptr)
	, a_save_next(       tr("Save and Open Next Image"), nullptr)
	, a_save_prev(       tr("Save and Open Previous Image"), nullptr)
	, a_next_fixable(    tr("Next fixable image"), nullptr)
	, a_prev_fixable(    tr("Previous fixable image"), nullptr)
	, a_go_to_number(    tr("&Go To File Number..."), nullptr)
	, a_open_session(    tr("Open Session"), nullptr)
	, a_save_session(    tr("Save Session"), nullptr)
	, a_fix_tags(        tr("&Apply Tag Fixes"), nullptr)
	, a_fetch_tags(      tr("Fetch Imageboard Tags..."), nullptr)
	, a_open_loc(        tr("Open &Containing Folder"), nullptr)
	, a_reload_tags(     tr("&Reload Tag File"), nullptr)
	, a_open_tags(       tr("Open Tag File &Location"), nullptr)
	, a_edit_tags(       tr("&Edit Tag File"), nullptr)
	, a_edit_temp_tags(  tr("Edit Temporary Tags..."), nullptr)
	, a_edit_mode(       QString{}, nullptr)
	, a_ib_replace(      tr("Re&place Imageboard Tags"), nullptr)
	, a_ib_restore(      tr("Re&store Imageboard Tags"), nullptr)
	, a_tag_forcefirst(  tr("&Force Author Tags First"), nullptr)
	, a_fit_to_screen(   tr("Fit to Screen"), nullptr)
	, a_navigate_by_wheel(tr("Switch files with Mouse Wheel"))
	, a_show_settings(   tr("P&references..."), nullptr)
	, a_view_normal(     tr("Show &WiseTagger"), nullptr)
	, a_view_minimal(    tr("Mi&nimal View"), nullptr)
	, a_view_statusbar(  tr("&Statusbar"), nullptr)
	, a_view_fullscreen( tr("&Fullscreen"), nullptr)
	, a_exit_fullscreen( tr("Exit &Fullscreen"), nullptr)
	, a_view_slideshow(  tr("Slide Sho&w"), nullptr)
	, a_exit_slideshow(  tr("Exit Slide Sho&w"), nullptr)
	, a_view_menu(       tr("&Menu"), nullptr)
	, a_view_input(      tr("Tag &Input"), nullptr)
	, a_view_sort_name(  tr("By File &Name"), nullptr)
	, a_view_sort_type(  tr("By File &Type"), nullptr)
	, a_view_sort_date(  tr("By Modification &Date"), nullptr)
	, a_view_sort_size(  tr("By File &Size"), nullptr)
	, a_view_sort_length(tr("By File Name &Length"), nullptr)
	, a_view_sort_tagcnt(tr("By Tag &Count"), nullptr)
	, a_play_pause(      tr("Play/Pause"))
	, a_play_mute(       tr("Mute"))
	, a_rotate_cw(       tr("Rotate Clockwise"))
	, a_rotate_ccw(      tr("Rotate Counter-Clockwise"))
	, a_about(           tr("&About..."), nullptr)
	, a_about_qt(        tr("About &Qt..."), nullptr)
	, a_help(            tr("&Help..."), nullptr)
	, a_stats(           tr("&Statistics..."), nullptr)
	, ag_sort_criteria(  nullptr)
	, menu_file(         tr("&File"))
	, menu_navigation(   tr("&Navigation"))
	, menu_view(         tr("&View"))
	, menu_play(         tr("&Play"))
	, menu_sort(         tr("&Sort Queue"))
	, menu_options(      tr("&Options"))
	, menu_commands(     tr("&Commands"))
	, menu_context_commands(tr("&Commands"))
	, menu_help(	     tr("&Help"))
	, menu_short_notification()
	, menu_notifications()
	, menu_tray()
	, m_statusbar()
	, m_statusbar_label()
	, m_tray_icon()
{
	setCentralWidget(&m_tagger);
	setAcceptDrops(true);
	updateStyle(); // NOTE: should be called before menus are created.
	updateWindowTitle();
	createActions();
	createMenus();
	createCommands();
	updateMenus();
	initSettings();
	QTimer::singleShot(50, this, &Window::parseCommandLineArguments); // NOTE: should be called later to avoid unnecessary media resize after process launch.

#ifdef Q_OS_WIN32
	QTimer::singleShot(1500, this, &Window::checkNewVersion);
#endif

}

//------------------------------------------------------------------------------
void Window::fileOpenDialog()
{

	const auto supported_image_formats = util::supported_image_formats_namefilter();
	const auto supported_video_formats = util::supported_video_formats_namefilter();

	auto fileNames = QFileDialog::getOpenFileNames(this,
		tr("Open File"),
		m_last_directory,
		tr("All Supported Files (%1);;Image Files (%2);;Video Files (%3)")
			.arg(util::join(supported_image_formats + supported_video_formats))
			.arg(util::join(supported_image_formats))
			.arg(util::join(supported_video_formats)));

	if(fileNames.size() == 1) {
		m_tagger.open(fileNames.first(), /*recursive=*/false);
	}
	if(fileNames.size() > 1) {
		m_tagger.queue().assign(fileNames, /*recursive=*/false);
		m_tagger.openFileInQueue();
	}
}

void Window::directoryOpenDialog(bool recursive)
{
	auto dir = QFileDialog::getExistingDirectory(nullptr,
	        recursive ? tr("Open Folder Recursively") : tr("Open Folder"),
		m_last_directory,
		QFileDialog::ShowDirsOnly);

	m_tagger.openDir(dir, recursive);
}

void Window::updateProxySettings()
{
	QSettings settings;
	settings.beginGroup(QStringLiteral("proxy"));
	bool proxy_enabled = settings.value(QStringLiteral("enabled"), false).toBool();
	bool use_system_proxy = settings.value("use_system").toBool();

	if(proxy_enabled) {
		if (use_system_proxy) {
			QNetworkProxyFactory::setUseSystemConfiguration(true);
		} else {
			QNetworkProxyFactory::setUseSystemConfiguration(false);
			auto protocol = settings.value(QStringLiteral("protocol")).toString();
			auto host = settings.value(QStringLiteral("host")).toString();
			auto port = settings.value(QStringLiteral("port")).toInt();
			QUrl proxy_url;
			proxy_url.setScheme(protocol);
			proxy_url.setHost(host);
			proxy_url.setPort(port);

			bool scheme_valid = (proxy_url.scheme() == QStringLiteral("http")
					  || proxy_url.scheme() == QStringLiteral("socks5"));

			if(!proxy_url.isValid() || proxy_url.port() == -1 || !scheme_valid)
			{
				QMessageBox::warning(nullptr,
					tr("Invalid proxy"),
					tr("<p>Proxy URL <em><code>%1</code></em> is invalid. Proxy <b>will not</b> be used!</p>").arg(proxy_url.toString()));
				return;
			}

			QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;
			if(proxy_url.scheme() == QStringLiteral("socks5"))
				type = QNetworkProxy::Socks5Proxy;
			else if(proxy_url.scheme() == QStringLiteral("http"))
				type = QNetworkProxy::HttpProxy;

			auto port_value = static_cast<uint16_t>(proxy_url.port());
			auto proxy = QNetworkProxy(type, proxy_url.host(), port_value);
			QNetworkProxy::setApplicationProxy(proxy);
		}
	} else {
		QNetworkProxyFactory::setUseSystemConfiguration(false);
	}
	settings.endGroup();
}


void Window::updateWindowTitle()
{
	if(m_tagger.isEmpty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		setWindowFilePath(QString{});
		return;
	}

	const auto media_dimensions = m_tagger.mediaDimensions();
	setWindowModified(m_tagger.fileModified());
	setWindowFilePath(m_tagger.currentFile());

	if (m_tagger.mediaIsVideo() || m_tagger.mediaIsAnimatedImage()) {
		setWindowTitle(tr(Window::MainWindowTitleFrameRate).arg(
			m_tagger.currentFileName(),
			QStringLiteral("[*]"),
			QString::number(media_dimensions.width()),
			QString::number(media_dimensions.height()),
			util::size::printable(m_tagger.mediaFileSize()),
			qApp->applicationVersion(),
			QString::number(m_tagger.mediaFramerate(), 'g', 2)));
	} else {
		setWindowTitle(tr(Window::MainWindowTitle).arg(
			m_tagger.currentFileName(),
			QStringLiteral("[*]"),
			QString::number(media_dimensions.width()),
			QString::number(media_dimensions.height()),
			util::size::printable(m_tagger.mediaFileSize()),
			qApp->applicationVersion()));
	}
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
		QStringLiteral("[*]"),
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

	QString left, right;

	if(m_show_current_directory) {
		auto last_modified = m_tagger.currentFileLastModified();
		left = tr("Zoom: %1%    Directory:  %2      Modified: %3 ago (%4)").arg(
		        QString::number(m_tagger.mediaZoomFactor() * 100.0f, 'f', 2),
			QDir::toNativeSeparators(m_tagger.currentDir()),
			util::duration(last_modified.secsTo(QDateTime::currentDateTime()), false),
			last_modified.toString("yyyy-MM-dd hh:mm:ss"));

		statusBar()->showMessage(left);
	}
	const auto current = m_tagger.queue().currentIndex() + 1u;
	const auto qsize   = m_tagger.queue().size();
	auto queue_filter  = m_tagger.queueFilter();
	if (queue_filter.isEmpty()) {
		m_statusbar_label.setText(QStringLiteral("%1 / %2  ")
			.arg(QString::number(current), QString::number(qsize)));
	} else {
		const bool matches = m_tagger.queue().currentFileMatchesQueueFilter();

		const auto current_filtered = m_tagger.queue().currentIndexFiltered() + 1u;
		const auto filtered_qsize = m_tagger.queue().filteredSize();
		auto color = m_statusbar_label.palette().color(QPalette::Disabled, QPalette::WindowText);

		m_statusbar_label.setText(tr(
			"<a href=\"#filter\" style=\"color:#0089cc;\">"
			"Queue Filter:</a>&nbsp;&nbsp;%1"
			"&nbsp;&nbsp;%2&nbsp;&nbsp;"
			"%3&nbsp;/&nbsp;%4&nbsp;&nbsp;%5")
		                          .arg(matches ? QStringLiteral("<b>%1</b>").arg(queue_filter) : queue_filter,
		                               matches ? "" : (filtered_qsize ? tr("(not matched)") : tr("(no matches)")),
		                               matches ? QString::number(current_filtered) : QString::number(current),
		                               matches ? QString::number(filtered_qsize)   : QString::number(qsize),
		                               matches ? QStringLiteral("|&nbsp;&nbsp;<span style=\"color: %3;\">%1 / %2</span>&nbsp;&nbsp;").arg(current).arg(qsize).arg(color.name()) : ""));
	}

	right = m_statusbar_label.text();

	QSettings settings;
	if (!m_statusbar.isVisible() && settings.value(SETT_SHOW_STATUS_MAIN, true).toBool()) {
		if (a_view_slideshow.isChecked())
			left.clear();
		m_tagger.setStatusText(left, right);
	} else {
		m_tagger.setStatusText(QString{}, QString{});
	}
}

void Window::updateImageboardPostURL(QString url)
{
	a_open_post.setDisabled(url.isEmpty());
	m_post_url = url;
}

void Window::addNotification(QString title, QString description, QString body)
{
	// remove other notifications of the same type
	removeNotification(title);
	// we want only last notification's action to be triggered
	QObject::disconnect(&m_tray_icon, &QSystemTrayIcon::messageClicked, nullptr, nullptr);

	if(!body.isEmpty()) {
		auto action = menu_notifications.addAction(title);

		connect(action, &QAction::triggered, this, [this,title,body]() {
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

void Window::removeNotification(QString title) {
	const auto actions = menu_notifications.actions();
	for(const auto a : qAsConst(actions)) {
		if(a && a->text() == title) {
			menu_notifications.removeAction(a);
			if(m_notification_count > 0) --m_notification_count;
		}
	}
	if(m_notification_count <= 0) {
		hideNotificationsMenu();
	}
	m_tray_icon.setVisible(!isVisible());
}

void Window::showShortNotificationMenu(QString title)
{
	menu_short_notification.setTitle(title);
	m_short_notification_display_timer.setSingleShot(true);
	m_short_notification_display_timer.start(8000);
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
	menu_notifications.setTitle(QStringLiteral(""));
}

void Window::showUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
	int percent_done = static_cast<int>(util::size::percent(bytesSent, bytesTotal));
#ifdef Q_OS_WIN32
	auto progress = m_win_taskbar_button.progress();
	progress->setVisible(true);
	progress->setMaximum(100);
	progress->setValue(percent_done);

	if(bytesSent == bytesTotal) {
		// set indicator to indeterminate mode
		progress->setMaximum(0);
		progress->setValue(0);
	}
#endif
	if(bytesTotal == 0)
		return;

	updateWindowTitleProgress(percent_done);
	statusBar()->showMessage(
		tr("Uploading %1 to iqdb.org...  %2% complete").arg(
			m_tagger.currentFileName(), QString::number(percent_done)));
}

void Window::showTagFetchProgress(QString url_str)
{
#ifdef Q_OS_WIN32
	auto progress = m_win_taskbar_button.progress();
	progress->setVisible(true);
	progress->setMaximum(0);
	progress->setValue(0);
#endif
	auto url = QUrl(url_str);
	statusBar()->showMessage(tr("Fetching tags from %1...").arg(url.host()));
}

void Window::showFileHashingProgress(QString file, int value)
{
	Q_UNUSED(file);
#ifdef Q_OS_WIN32
	auto progress = m_win_taskbar_button.progress();
	progress->setVisible(true);
	progress->setMaximum(100);
	progress->setValue(value);
#endif
	updateWindowTitleProgress(value);
	statusBar()->showMessage(tr("Calculating file hash...  %1% complete").arg(value));
}

void Window::showCommandExecutionProgress(QString command_name)
{
#ifdef Q_OS_WIN32
	auto progress = m_win_taskbar_button.progress();
	progress->setVisible(true);
	progress->setMaximum(0);
	progress->setValue(0);
#endif
	statusBar()->showMessage(tr("Running %1...").arg(command_name));
}

void Window::hideUploadProgress()
{
#ifdef Q_OS_WIN32
	m_win_taskbar_button.progress()->setVisible(false);
#endif
	updateWindowTitle();
	statusBar()->showMessage(tr("Done."), 3000);
}

void Window::alert()
{
	auto window = windowHandle();
	Q_ASSERT(window != nullptr);
	window->alert(2500);
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
		bool recursive_enabled = qApp->keyboardModifiers() & Qt::SHIFT;
		const auto drop_event = static_cast<QDropEvent*>(e);
		const auto fileurls = drop_event->mimeData()->urls();
		QFileInfo dropfile;

		Q_ASSERT(!fileurls.empty());

		if(fileurls.size() == 1) {
			dropfile.setFile(fileurls.first().toLocalFile());
			m_tagger.open(dropfile.absoluteFilePath(), recursive_enabled);
			return true;
		}

		// NOTE: allow user to choose to rename or cancel.
		if(m_tagger.rename() == Tagger::RenameStatus::Cancelled)
			return true;

		QStringList filelist;
		for(const auto& fileurl : qAsConst(fileurls)) {
			filelist.push_back(fileurl.toLocalFile());
		}
		m_tagger.queue().assign(filelist, recursive_enabled);
		m_tagger.openFileInQueue();
		return true;
	}
	return false;
}

void Window::closeEvent(QCloseEvent *e)
{
	if(m_tagger.fileModified()) {
		if(Tagger::canExit(m_tagger.rename())) {
			saveSettings();
			QMainWindow::closeEvent(e);
			return;
		}
		bool was_hidden = !isVisible();
		if (was_hidden) show(); // BUG: ignore() doesn't work if the window is hidden.
		e->ignore();
		if (was_hidden) hide();
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
	m_tagger.playMedia();
	e->accept();
}

void Window::hideEvent(QHideEvent *e)
{
	m_tagger.pauseMedia();
	e->accept();
}

void Window::mousePressEvent(QMouseEvent * ev)
{
	if (ev->buttons() & Qt::ForwardButton) {
		a_next_file.trigger();
		ev->accept();
	}

	if (ev->buttons() & Qt::BackButton) {
		a_prev_file.trigger();
		ev->accept();
	}
	QMainWindow::mousePressEvent(ev);
}

void Window::wheelEvent(QWheelEvent *ev)
{
	QSettings settings;
	bool modifier_not_required = settings.value(SETT_NAVIGATE_BY_WHEEL, false).toBool();
	if (modifier_not_required 
		|| ev->modifiers() & Qt::ControlModifier 
		|| ev->modifiers() & Qt::ShiftModifier) {
		ev->accept();
		auto delta = ev->angleDelta().y();
		if (delta > 0) {
			a_next_file.trigger();
		}
		if (delta < 0) {
			a_prev_file.trigger();
		}
	}
	QMainWindow::wheelEvent(ev);
}


//------------------------------------------------------------------------------
void Window::parseCommandLineArguments()
{
	const auto args = qApp->arguments(); // slow

	if(args.size() <= 1)
		return;

	QStringList paths;
	bool recursive = false;

	for(int i = 1; i < args.size(); ++i) {
		const auto& arg = args.at(i);
		if(arg.startsWith(QChar('-'))) {
			if (arg == QStringLiteral("--restart")) {
				readRestartData();
				return;
			}

			if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help")) {
				QMessageBox::information(this,
				                         tr("%1 Command Line Usage").arg(TARGET_PRODUCT),
				                         tr("<h4><pre>%1 [-r | --recursive] [-h | --help] [path...]</pre></h4>"
				                            "<table>"
				                            "<tr><td style=\"padding: 5px 10px\"><code>-r, --recursive</code></td><td style=\"padding: 5px 10px\">Open files in subdirectories recursively.</td></tr>"
				                            "<tr><td style=\"padding: 5px 10px\"><code>-h, --help</code></td><td style=\"padding: 5px 10px\">Display this help message.</td></tr>"
				                            "<tr><td style=\"padding: 5px 10px\"><code>path...</code></td><td style=\"padding: 5px 10px\">One or several file/directory paths.<br/>Use \"-\" to read paths from <code>stdin</code>.</td></tr>"
				                            "</table>").arg(TARGET_PRODUCT));
			}

			if (arg == QStringLiteral("-r") || arg == QStringLiteral("--recursive")) {
				recursive = true;
			}

			continue;
		}
		paths.append(arg);
	}

	if (paths.size() == 1) {
		m_tagger.open(paths[0], recursive); // may open session file or read list from stdin
		return;
	}

	m_tagger.queue().assign(paths, recursive);
	m_tagger.queue().select(0); // must select 0 before sorting to always open first argument
	m_tagger.queue().sort();
	m_tagger.openFileInQueue(m_tagger.queue().currentIndex()); // sorting changed the index of selected file
}

//------------------------------------------------------------------------------
void Window::initSettings()
{
	QSettings settings;

	m_last_directory = settings.value(SETT_LAST_DIR).toString();
	m_view_mode      = settings.value(SETT_VIEW_MODE).value<ViewMode>();
	bool show_status = settings.value(SETT_SHOW_STATUS, false).toBool();
	bool show_menu   = settings.value(SETT_SHOW_MENU,   true).toBool();
	bool show_input  = settings.value(SETT_SHOW_INPUT,  true).toBool();
	bool nav_wheel   = settings.value(SETT_NAVIGATE_BY_WHEEL, false).toBool();
	bool fit_screen  = settings.value(SETT_FIT_TO_SCREEN, false).toBool();

	bool restored_geo   = restoreGeometry(settings.value(SETT_WINDOW_GEOMETRY).toByteArray());
	bool restored_state = restoreState(settings.value(SETT_WINDOW_STATE).toByteArray());

	if(!restored_state || !restored_geo) {
		resize(1024,640);
		move(100,100);
		showNormal();
	}

	menuBar()->setVisible(show_menu);
	m_tagger.setViewMode(m_view_mode);
	m_tagger.setInputVisible(show_input);
	m_statusbar.setVisible(show_status && m_view_mode != ViewMode::Minimal);

	a_view_fullscreen.setChecked(isFullScreen());
	a_fit_to_screen.setChecked(fit_screen);
	a_view_slideshow.setChecked(isFullScreen() && !show_menu && !show_input && !show_status);
	a_view_minimal.setChecked(m_view_mode == ViewMode::Minimal);
	a_view_statusbar.setChecked(show_status);
	a_view_menu.setChecked(show_menu);
	a_view_input.setChecked(show_input);
	a_navigate_by_wheel.setChecked(nav_wheel);

	a_play_mute.setChecked(settings.value(SETT_PLAY_MUTE, false).toBool());
	m_tagger.setMediaMuted(a_play_mute.isChecked());

	a_ib_replace.setChecked(settings.value(SETT_REPLACE_TAGS, false).toBool());
	a_ib_restore.setChecked(settings.value(SETT_RESTORE_TAGS, true).toBool());
	a_tag_forcefirst.setChecked(settings.value(SETT_FORCE_AUTHOR_FIRST, false).toBool());

	m_show_current_directory = settings.value(SETT_SHOW_CURRENT_DIR, true).toBool();

	auto edit_mode = settings.value(SETT_EDIT_MODE).value<EditMode>();
	setEditMode(edit_mode);

	m_tagger.setUpscalingEnabled(fit_screen);

	updateProxySettings();
}

void Window::saveSettings()
{
	// FIXME: hack to prevent saving slideshow state
	if (a_view_slideshow.isChecked())
		setSlideShow(false);

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
	setViewMode(m_view_mode);
	for(auto action : menu_commands.actions()) {
		this->removeAction(action); // NOTE: to prevent hotkey conflicts
	}
	menu_commands.clear();
	menu_context_commands.clear();
	createCommands();
	updateMenus();
}

void Window::restartProcess(QString current_text)
{
	Q_ASSERT(!m_tagger.fileModified());
	class DetachedProc : public QProcess {
	public:
		using QProcess::QProcess;
		void detach() {
			waitForStarted();
			setProcessState(QProcess::NotRunning);
		}
	};

	DetachedProc proc;
	QStringList args;
	if (!m_tagger.isEmpty()) {
		args.append(QStringLiteral("--restart"));
	}

	proc.start(qApp->applicationFilePath(), args, QIODevice::WriteOnly);
	if (!proc.waitForStarted()) {
		pwarn << "could not start new process";

		// restore modifications
		m_tagger.setText(current_text);
		return;
	}

	if (!m_tagger.isEmpty()) {

		QByteArray header;
		{
			QBuffer buffer(&header);
			buffer.open(QIODevice::WriteOnly);
			QTextStream out(&buffer);
			out.setCodec("UTF-8");
			// version
			out << 1 << '\n';
			// current text
			out << current_text << '\n';
			// queue filter
			out << m_tagger.queueFilter() << '\n';
			// end of header
			out << "--EOH--" << '\n';
			out.flush();
			buffer.close();
		}

		auto written = proc.write(header);
		if (written < 0 || written != header.size()) {
			pwarn << "error writing session header to process stdin";
		} else {

			// serialize queue and write to stdin of the process
			auto data = m_tagger.queue().saveToMemory();
			written = proc.write(data);
			if (written < 0 || written != data.size()) {
				pwarn << "error writing session data to process stdin";
			}
		}
		proc.waitForBytesWritten();
	}
	proc.closeWriteChannel();

	if (proc.state() == QProcess::Running && proc.error() == QProcess::UnknownError) {
		bool closed = close();
		Q_ASSERT(closed);
		proc.detach();
	}
}

void Window::readRestartData()
{
	QFile in;
	in.open(stdin, QIODevice::ReadOnly);
	QTextStream stream(&in);
	stream.setCodec("UTF-8");

	auto version = stream.readLine(10);
	if (version != QStringLiteral("1")) {
		pwarn << "Unknown input data version:" << version;
		return;
	}
	auto current_text = stream.readLine(10000);
	auto queue_filter = stream.readLine(10000);

	auto end = stream.readLine(10);
	if (end != QStringLiteral("--EOH--")) {
		pwarn << "No end of headers found";
		return;
	}

	auto session_data = in.readAll();
	m_tagger.openSession(session_data);
	m_tagger.setQueueFilter(queue_filter);
	m_tagger.setText(current_text);
	updateStatusBarText();
}

void Window::scheduleRestart()
{
	// remember current tags
	auto current_text = m_tagger.text();

	// undo tag modification to prevent rename dialog
	m_tagger.resetText();
	// launch process
	restartProcess(current_text);
}

void Window::updateStyle()
{
	qApp->setWindowIcon(QIcon(QStringLiteral(":/wisetagger.svg")));
	m_tray_icon.setIcon(QPixmap(QStringLiteral(":/wisetagger.svg"))); // BUG: cannot use QIcon on linux, but QPixmap works (might be related to QTBUG-55932)
}

void Window::setViewMode(ViewMode mode)
{
	if(mode == ViewMode::Minimal) {
		m_statusbar.hide();
	} else {
		m_statusbar.setVisible(a_view_statusbar.isChecked());
	}
	m_tagger.setViewMode(mode);
}

void Window::setEditMode(EditMode mode)
{
	m_tagger.setEditMode(mode);
	auto get_mode_str = [](EditMode mode){
		QString current_mode_str;
		switch(mode) {
		case EditMode::Naming:
			current_mode_str = tr("Naming mode");
			break;
		case EditMode::Tagging:
			current_mode_str = tr("Tagging mode");
			break;
		}
		return current_mode_str;
	};

	auto get_mode_icon = [](EditMode mode){
		QIcon res;
		switch(mode) {
		case EditMode::Naming:
			break;
		case EditMode::Tagging:
			res = QIcon::fromTheme("tag", util::fallbackIcon("tag"));
			break;
		}
		return res;
	};

	showShortNotificationMenu(get_mode_str(mode));
	auto next_mode = GlobalEnums::next_edit_mode(mode);
	a_edit_mode.setText(get_mode_str(next_mode));
	a_edit_mode.setIcon(get_mode_icon(next_mode));
	updateMenus();
}

void Window::setSlideShow(bool slide_show)
{
	a_view_input.setEnabled(!slide_show);
	a_view_statusbar.setEnabled(!slide_show);
	a_view_fullscreen.setEnabled(!slide_show);
	a_view_minimal.setEnabled(!slide_show);

	a_view_menu.setChecked(!slide_show);
	a_view_input.setChecked(!slide_show);
	a_view_statusbar.setChecked(!slide_show);
	a_view_fullscreen.setChecked(slide_show);

	QSettings settings;
	m_tagger.setUpscalingEnabled(slide_show || settings.value(SETT_FIT_TO_SCREEN).toBool());
}

#ifdef Q_OS_WIN
namespace logging_category {
	Q_LOGGING_CATEGORY(vercheck, "Window.VersionCheck")
}
#define vcwarn qCWarning(logging_category::vercheck)
#define vcdbg  qCDebug(logging_category::vercheck)
void Window::checkNewVersion()
{
	QSettings settings;
	auto last_checked = settings.value(SETT_LAST_VER_CHECK,QDate(2016,1,1)).toDate();
	auto checking_disabled = !settings.value(SETT_VER_CHECK_ENABLED, true).toBool();

	if(checking_disabled || last_checked == QDate::currentDate()) {
		vcdbg << (checking_disabled ? "vercheck disabled" : "vercheck enabled") << last_checked.toString();
		return;
	}

	connect(&m_vernam, &QNetworkAccessManager::finished, this, &Window::processNewVersion);

	QNetworkRequest req{VER_CHECK_URL};
	req.setHeader(QNetworkRequest::UserAgentHeader, WISETAGGER_USERAGENT);
	m_vernam.get(req);
}

void Window::processNewVersion(QNetworkReply *r)
{
	// use unique_ptr with custom deleter to clean up the request object
	auto guard = std::unique_ptr<QNetworkReply, std::function<void(QNetworkReply*)>>(r, [](auto ptr)
	{
		ptr->deleteLater();
	});


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
	auto parts = util::split(response);
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
		addNotification(tr("New version available"),
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
	QSettings settings;

	auto size = settings.beginReadArray(SETT_COMMANDS_KEY);
	for(auto i{0}; i < size; ++i) {
		settings.setArrayIndex(i);
		auto name = settings.value(SETT_COMMAND_NAME).toString();
		auto cmd  = settings.value(SETT_COMMAND_CMD).toStringList();
		auto hkey = settings.value(SETT_COMMAND_HOTKEY).toString();
		auto mode = settings.value(SETT_COMMAND_MODE).value<CommandOutputMode>();

		if(name == CMD_SEPARATOR) {
			menu_commands.addSeparator();
			menu_context_commands.addSeparator();
			continue;
		}

		if(name.isEmpty() || cmd.isEmpty()) {
			continue;
		}

		auto binary = cmd.first();
		cmd.removeFirst();

		auto action = menu_commands.addAction(name);
		if(!QFile::exists(binary)) {
			pwarn << "Invalid command: executable does not exist:" << binary;
			action->setText(QStringLiteral("! %1").arg(action->text()));
			action->setToolTip(tr("Executable \"%1\" does not exist!").arg(binary));
			action->setEnabled(false);
			continue;
		}

		this->addAction(action); // NOTE: to make hotkeys work when menubar is hidden, does not take ownership
		menu_context_commands.addAction(action);
		action->setIcon(util::get_icon_from_executable(binary));
		if(!hkey.isEmpty()) {
			action->setShortcut(hkey);
		}

		connect(action, &QAction::triggered, this, [this,name,binary,cmd,mode]()
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
			if (mode == CommandOutputMode::Ignore) {
				bool success = QProcess::startDetached(binary, cmd_tmp, m_tagger.currentDir());
				pdbg << "QProcess::startDetached(" << binary << "," << cmd_tmp << ") =>" << success;
				if(!success) {
					QMessageBox::critical(this,
					        tr("Failed to start command"),
					        tr("<p>Failed to launch command <b>%1</b>:</p><p>Could not start <code>%2</code>.</p>")
					                .arg(name, binary));
				}
			} else {

				auto proc = new QProcess{this};
				proc->setProgram(binary);
				proc->setArguments(cmd_tmp);
				proc->setWorkingDirectory(m_tagger.currentDir());

				showCommandExecutionProgress(name);

				connect(&m_tagger,
				        &Tagger::fileOpened,
				        proc,
				        [this, proc](const QString&)
				{
					if (proc->state() != QProcess::NotRunning) {
						disconnect(proc, nullptr, this, nullptr);
						proc->terminate();
					}
				});

				connect(proc,
				        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
				        this,
				        [this, name, binary, mode](int exit_code, QProcess::ExitStatus exit_status)
				{
					Q_UNUSED(exit_code);
					
					QClipboard *clipboard = QGuiApplication::clipboard();
					Q_ASSERT(clipboard != nullptr);

					if (exit_status != QProcess::NormalExit) {
						QMessageBox::critical(this,
						        tr("Failed to start command"),
						        tr("<p>Failed to launch command <b>%1</b>:</p><p>Could not start <code>%2</code>.</p>")
						                .arg(name, binary));


						hideUploadProgress();
						alert();
						return;
					}

					auto proc = qobject_cast<QProcess*>(sender());
					Q_ASSERT(proc);

					auto tags_data = proc->readAllStandardOutput();

					QTextStream in{tags_data};
					in.setCodec("UTF-8");

					auto raw = in.readAll();
					auto tags = raw.replace('\n', ' ').trimmed();
					if (!tags.isEmpty()) {
						auto current_tags = m_tagger.text();

						switch(mode) {
						case CommandOutputMode::Replace:
							m_tagger.addTags(tags);
							break;
						case CommandOutputMode::Append:
							m_tagger.setText(current_tags + " " + tags);
							break;
						case CommandOutputMode::Prepend:
							m_tagger.setText(tags + " " + current_tags);
							break;
						case CommandOutputMode::Copy:
							clipboard->setText(raw);
							addNotification(tr("Command Executed"), tr("Results copied to clipboard"));
							break;
						case CommandOutputMode::Show:
							addNotification(tr("Command Output"), raw, raw);
							break;
						default:
							break;
						}
					}
					hideUploadProgress();
					proc->deleteLater();
				}, Qt::QueuedConnection);

				proc->start(QProcess::ReadOnly);

			}
		});
	}
	settings.endArray();
}

//------------------------------------------------------------------------------
void Window::createActions()
{
	a_open_file.setShortcut(    QKeySequence::Open);
	a_open_dir.setShortcut(     QKeySequence(tr("Ctrl+D", "File|Open Directory")));
	a_open_dir_recurse.setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D));
	a_next_file.setShortcut(    QKeySequence(Qt::Key_Right));
	a_prev_file.setShortcut(    QKeySequence(Qt::Key_Left));
	a_set_queue_filter.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
	a_save_file.setShortcut(    QKeySequence::Save);
	a_save_next.setShortcut(    QKeySequence(Qt::SHIFT + Qt::Key_Right));
	a_save_prev.setShortcut(    QKeySequence(Qt::SHIFT + Qt::Key_Left));
	a_next_fixable.setShortcut( QKeySequence(Qt::ALT + Qt::Key_Right));
	a_prev_fixable.setShortcut( QKeySequence(Qt::ALT + Qt::Key_Left));
	a_delete_file.setShortcut(  QKeySequence::Delete);
	a_copy_file.setShortcut(    QKeySequence(tr("Ctrl+C", "Copy to clipboard")));
	a_fix_tags.setShortcut(     QKeySequence(tr("Ctrl+A", "Fix tags")));
	a_open_post.setShortcut(    QKeySequence(tr("Ctrl+P", "Open post")));
	a_iqdb_search.setShortcut(  QKeySequence(tr("Ctrl+F", "Reverse search")));
	a_fetch_tags.setShortcut(   QKeySequence(tr("Ctrl+Shift+F", "Fetch tags")));
	a_open_loc.setShortcut(	    QKeySequence(tr("Ctrl+L", "Open file location")));
	a_help.setShortcut(         QKeySequence::HelpContents);
	a_exit.setShortcut(         QKeySequence::Close);
	a_view_fullscreen.setShortcut(QKeySequence::FullScreen);
	a_view_slideshow.setShortcut(QKeySequence(Qt::Key_F5));
	a_hide.setShortcut(QKeySequence{Qt::Key_Escape, Qt::Key_Escape});
	a_hide.setShortcutContext(Qt::WidgetShortcut);
	a_view_menu.setShortcut(    QKeySequence(Qt::CTRL + Qt::Key_M));
	a_view_input.setShortcut(   QKeySequence(Qt::CTRL + Qt::Key_I));
	a_view_statusbar.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
	a_play_pause.setShortcut(   QKeySequence(Qt::Key_Space));
	a_play_mute.setShortcut(    QKeySequence(Qt::Key_M));
	a_go_to_number.setShortcut( QKeySequence(Qt::CTRL + Qt::Key_NumberSign));
	a_edit_mode.setShortcut(    QKeySequence(Qt::Key_F2));
	a_rotate_cw.setShortcut(    QKeySequence(Qt::CTRL + Qt::Key_Period));
	a_rotate_ccw.setShortcut(   QKeySequence(Qt::CTRL + Qt::Key_Comma));
	a_fit_to_screen.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_0));
	a_navigate_by_wheel.setShortcut(QKeySequence(Qt::Key_ScrollLock));

	a_view_sort_name.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_N));
	a_view_sort_type.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_T));
	a_view_sort_date.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_D));
	a_view_sort_size.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_Z));
	a_view_sort_length.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_L));
	a_view_sort_tagcnt.setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_S, Qt::Key_C));

	a_open_dir_recurse.setStatusTip(tr("Open all files in the folder and all subfolders."));
	a_open_post.setStatusTip(     tr("Open imageboard post of this image."));
	a_iqdb_search.setStatusTip(   tr("Upload this image to iqdb.org and open search results page in default browser."));
	a_open_loc.setStatusTip(      tr("Open folder where this image is located."));
	a_open_tags.setStatusTip(     tr("Open folder where current tag file is located."));
	a_reload_tags.setStatusTip(   tr("Reload changes in tag files and search for newly added tag files."));
	a_edit_tags.setStatusTip(     tr("Open current tag file in default text editor."));
	a_ib_replace.setStatusTip(    tr("Toggle replacing certain imageboard tags with their shorter version."));
	a_ib_restore.setStatusTip(    tr("Toggle restoring imageboard tags back to their original version."));
	a_tag_forcefirst.setStatusTip(tr("Toggle forcing the author tag to be the first tag in the filename."));

	a_ib_replace.setCheckable(true);
	a_ib_restore.setCheckable(true);
	a_tag_forcefirst.setCheckable(true);
	a_fit_to_screen.setCheckable(true);
	a_navigate_by_wheel.setCheckable(true);
	a_view_statusbar.setCheckable(true);
	a_view_fullscreen.setCheckable(true);
	a_view_slideshow.setCheckable(true);
	a_view_minimal.setCheckable(true);
	a_view_menu.setCheckable(true);
	a_view_input.setCheckable(true);
	a_play_pause.setCheckable(true);
	a_play_mute.setCheckable(true);

	a_view_fullscreen.setIcon(QIcon::fromTheme("view-fullscreen", util::fallbackIcon("view-fullscreen")));
	a_view_slideshow.setIcon(QIcon::fromTheme("view-presentation"));
	a_set_queue_filter.setIcon(QIcon::fromTheme("view-filter", util::fallbackIcon("view-filter")));
	a_fit_to_screen.setIcon(QIcon::fromTheme("zoom-fit-best", util::fallbackIcon("zoom-fit-best")));
	a_rotate_cw.setIcon(QIcon::fromTheme("object-rotate-right", util::fallbackIcon("object-rotate-right")));
	a_rotate_ccw.setIcon(QIcon::fromTheme("object-rotate-left", util::fallbackIcon("object-rotate-left")));
	a_next_file.setIcon(QIcon::fromTheme("go-next", util::fallbackIcon("go-next")));
	a_prev_file.setIcon(QIcon::fromTheme("go-previous", util::fallbackIcon("go-previous")));
	a_iqdb_search.setIcon(QIcon::fromTheme("edit-find", util::fallbackIcon("edit-find")));
	a_delete_file.setIcon(QIcon::fromTheme("edit-delete", util::fallbackIcon("edit-delete")));
	a_fetch_tags.setIcon(util::fallbackIcon("download-cloud"));

	connect(&m_reverse_search, &ReverseSearch::uploadProgress, this, &Window::showUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, &Window::hideUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, &Window::alert);
	connect(&m_reverse_search, &ReverseSearch::reverseSearched, &TaggerStatistics::instance(), &TaggerStatistics::reverseSearched);
	connect(&m_reverse_search, &ReverseSearch::reverseSearched, this, [this]()
	{
		addNotification(tr("IQDB upload finished"), tr("Search results page opened in default browser."));
	});

	connect(&a_open_file,   &QAction::triggered, this, &Window::fileOpenDialog);
	connect(&a_open_dir,    &QAction::triggered, this, [this](){
		directoryOpenDialog(false);
	});
	connect(&a_open_dir_recurse, &QAction::triggered, this, [this](){
		directoryOpenDialog(true);
	});
	connect(&a_exit,        &QAction::triggered, this, &Window::close);
	connect(&a_hide,        &QAction::triggered, this, &Window::hide);
	connect(&a_hide,        &QAction::triggered, &m_tray_icon, &QSystemTrayIcon::show);
	connect(&m_tray_icon,   &QSystemTrayIcon::activated, this, [this](auto reason)
	{
		if (reason == QSystemTrayIcon::DoubleClick) {
			a_view_normal.trigger();
		}
	});
	connect(&m_tagger,      &Tagger::hideRequested, &a_hide, &QAction::trigger);
	connect(&a_view_normal, &QAction::triggered, this, &Window::show);
	connect(&a_about,       &QAction::triggered, this, &Window::about);
	connect(&a_about_qt,    &QAction::triggered, qApp, &QApplication::aboutQt);
	connect(&a_help,        &QAction::triggered, this, &Window::help);
	connect(&a_delete_file, &QAction::triggered, &m_tagger, &Tagger::deleteCurrentFile);
	connect(&a_copy_file,   &QAction::triggered, this, [this](){
		auto current = m_tagger.currentFile();

		auto mime = new QMimeData{};
		mime->setUrls({QUrl::fromLocalFile(current)});
		QApplication::clipboard()->setMimeData(mime);
	});
	connect(&a_fix_tags,    &QAction::triggered, &m_tagger, &Tagger::fixTags);

	connect(&a_reload_tags, &QAction::triggered, &m_tagger, &Tagger::reloadTags);
	connect(&a_open_tags,   &QAction::triggered, &m_tagger, &Tagger::openTagFilesInShell);
	connect(&a_edit_tags,   &QAction::triggered, &m_tagger, &Tagger::openTagFilesInEditor);
	connect(&a_edit_temp_tags, &QAction::triggered, &m_tagger, &Tagger::openTempTagFileEditor);
	connect(&a_edit_mode,   &QAction::triggered, this, [this](){
		auto next_mode = GlobalEnums::next_edit_mode(m_tagger.editMode());
		setEditMode(next_mode);
		QSettings settings;
		settings.setValue(SETT_EDIT_MODE, QVariant::fromValue(next_mode));
	});
	connect(&a_stats,       &QAction::triggered,   &TaggerStatistics::instance(), &TaggerStatistics::showStatsDialog);
	connect(&m_tagger,      &Tagger::tagsEdited,   this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened,   this, &Window::updateMenus);
	connect(&m_tagger,      &Tagger::cleared,      this, &Window::updateMenus);
	connect(&m_tagger,      &Tagger::fileOpened,   this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::cleared,      this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened,   this, &Window::updateStatusBarText);
	connect(&m_tagger,      &Tagger::cleared,      this, &Window::updateStatusBarText);
	connect(&m_tagger,      &Tagger::mediaResized, this, &Window::updateStatusBarText);
	connect(&m_tagger.queue(), &FileQueue::newFilesAdded, this, &Window::updateStatusBarText);
	connect(&m_tagger.tag_fetcher(), &TagFetcher::hashing_progress, this, &Window::showFileHashingProgress);
	connect(&m_tagger.tag_fetcher(), &TagFetcher::started, this, &Window::showTagFetchProgress);
	connect(&m_tagger.tag_fetcher(), &TagFetcher::aborted, this, &Window::hideUploadProgress);
	connect(&m_tagger.tag_fetcher(), &TagFetcher::failed, this, [this](auto file, auto reason)
	{
		if (file == m_tagger.currentFile()) {
			hideUploadProgress();
			alert();
			addNotification(tr("Tag fetching failed"), reason);
			statusBar()->showMessage(tr("Tag fetching failed:  %1").arg(reason), 3000);
		}
	});

	connect(&m_tagger,      &Tagger::tagFileChanged, this, [this]()
	{
		addNotification(tr("Tag file changed"),
		                tr("Tag file has been edited or removed.\n"
		                   "All changes successfully applied."));
	});
	connect(&m_tagger,      &Tagger::fileOpened, this, [this]()
	{
		updateImageboardPostURL(m_tagger.postURL());
	});
	connect(&a_save_file,   &QAction::triggered, this, [this]()
	{
		m_tagger.rename(Tagger::RenameOption::ReopenFile);
	});
	connect(&m_filter_dialog, &FilterDialog::finished, this, [this](int result){

		auto dialog = qobject_cast<FilterDialog*>(sender());
		Q_ASSERT(dialog);
		pdbg << result;

		if (result == QDialog::Accepted) {
			m_tagger.setQueueFilter(dialog->text());
		}
		updateStatusBarText();
	});
	auto set_queue_filter = [this](auto _)
	{
		Q_UNUSED(_);
		m_filter_dialog.setModel(m_tagger.completionModel());
		m_filter_dialog.setText(m_tagger.queueFilter());
		m_filter_dialog.open();
	};
	connect(&a_set_queue_filter, &QAction::triggered, this, set_queue_filter);
	connect(&m_statusbar_label, &QLabel::linkActivated, this, set_queue_filter);
	connect(&m_tagger, &Tagger::linkActivated, this, [this, set_queue_filter](auto link){
		if (link == QStringLiteral("#open_file")) {
			fileOpenDialog();
		}
		if (link == QStringLiteral("#open_dir")) {
			bool recursive_enabled = qApp->keyboardModifiers() & Qt::SHIFT;
			directoryOpenDialog(recursive_enabled);
		}
		if (link == QStringLiteral("#filter")) {
			set_queue_filter(link);
		}
	});
	connect(&a_next_file,   &QAction::triggered, this, [this]()
	{
		m_tagger.nextFile();
	});
	connect(&a_prev_file,   &QAction::triggered, this, [this]()
	{
		m_tagger.prevFile();
	});
	connect(&a_save_next,   &QAction::triggered, this, [this]()
	{
		m_tagger.nextFile(Tagger::RenameOption::ForceRename);
	});
	connect(&a_save_prev,   &QAction::triggered, this, [this]()
	{
		m_tagger.prevFile(Tagger::RenameOption::ForceRename);
	});
	connect(&a_next_fixable,   &QAction::triggered, this, [this]()
	{
		m_tagger.nextFile(Tagger::RenameOption::NoOption, Tagger::SkipOption::SkipToFixable);
	});
	connect(&a_prev_fixable,   &QAction::triggered, this, [this]()
	{
		m_tagger.prevFile(Tagger::RenameOption::NoOption, Tagger::SkipOption::SkipToFixable);
	});
	connect(&a_open_post,   &QAction::triggered, this, [this]()
	{
		QDesktopServices::openUrl(m_post_url);
	});
	connect(&a_iqdb_search, &QAction::triggered, this, [this]()
	{
		m_reverse_search.search(m_tagger.currentFile());
	});
	connect(&a_open_loc,    &QAction::triggered, this, [this]()
	{
		util::open_files_in_gui_shell(QStringList{m_tagger.currentFile()});
	});
	connect(&a_ib_replace,  &QAction::triggered, [](bool checked)
	{
		QSettings settings; settings.setValue(SETT_REPLACE_TAGS, checked);
	});
	connect(&a_ib_restore,  &QAction::triggered, [](bool checked)
	{
		QSettings settings; settings.setValue(SETT_RESTORE_TAGS, checked);
	});
	connect(&a_tag_forcefirst,  &QAction::triggered, [](bool checked)
	{
		QSettings settings; settings.setValue(SETT_FORCE_AUTHOR_FIRST, checked);
	});
	connect(&a_navigate_by_wheel,  &QAction::triggered, [](bool checked)
	{
		QSettings settings; settings.setValue(SETT_NAVIGATE_BY_WHEEL, checked);
	});	
	connect(&a_fit_to_screen,  &QAction::triggered, [this](bool checked)
	{
		QSettings settings; settings.setValue(SETT_FIT_TO_SCREEN, checked);
		m_tagger.setUpscalingEnabled(checked);
	});

	connect(&a_view_statusbar, &QAction::toggled, this, [this](bool checked)
	{
		QSettings settings; settings.setValue(SETT_SHOW_STATUS, checked);
		m_statusbar.setVisible(checked && m_view_mode != ViewMode::Minimal);
	});
	connect(&a_view_fullscreen, &QAction::toggled, this, [this](bool checked)
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
		QSettings settings;
		m_tagger.setUpscalingEnabled(checked || settings.value(SETT_FIT_TO_SCREEN).toBool());
	});
	connect(&a_exit_fullscreen, &QAction::triggered, this, [this]() {
		a_view_fullscreen.setChecked(false);
	});
	connect(&a_view_slideshow, &QAction::triggered, this, &Window::setSlideShow);
	connect(&a_exit_slideshow, &QAction::triggered, this, [this](bool) {
		if (a_view_slideshow.isChecked()) {
			a_view_slideshow.setChecked(false);
			setSlideShow(false);
		}
	});

	connect(&a_view_minimal, &QAction::triggered, this, [this](bool checked)
	{
		m_view_mode = checked ? ViewMode::Minimal : ViewMode::Normal;
		QSettings settings; settings.setValue(SETT_VIEW_MODE, QVariant::fromValue(m_view_mode));
		updateSettings();
	});
	connect(&a_view_menu, &QAction::toggled, this, [this](bool checked)
	{
		QSettings settings; settings.setValue(SETT_SHOW_MENU, checked);
		this->menuBar()->setVisible(checked);
	});
	connect(&a_view_input, &QAction::toggled, this, [this](bool checked)
	{
		QSettings settings; settings.setValue(SETT_SHOW_INPUT, checked);
		this->m_tagger.setInputVisible(checked);
	});
	connect(&a_play_pause, &QAction::triggered, &m_tagger, &Tagger::setMediaPlaying);
	connect(&a_play_mute, &QAction::triggered, this, [this](bool checked) {
		QSettings settings; settings.setValue(SETT_PLAY_MUTE, checked);
		m_tagger.setMediaMuted(checked);
	});
	connect(&a_rotate_cw, &QAction::triggered, this, [this](bool) {
		m_tagger.rotateImage(true);
	});
	connect(&a_rotate_ccw, &QAction::triggered, this, [this](bool) {
		m_tagger.rotateImage(false);
	});
	connect(&a_save_session, &QAction::triggered, this, [this]()
	{
		auto filename = QFileDialog::getSaveFileName(this,
			tr("Save Session"),
			m_last_directory,
			tr("Session Files (%1)").arg(FileQueue::sessionExtensionFilter));
		if(filename.isEmpty())
			return;

		if(!m_tagger.queue().saveToFile(filename)) {
			QMessageBox::critical(this,
				tr("Save Session Error"),
				tr("<p>Could not save session to <b>%1</b>.</p><p>Check file permissions.</p>").arg(filename));
			return;
		}
		QSettings settings;
		settings.setValue(SETT_LAST_SESSION_DIR, QFileInfo(filename).absolutePath());
	});
	connect(&a_open_session, &QAction::triggered, this, [this]()
	{
		QSettings settings;
		auto lastdir = settings.value(SETT_LAST_SESSION_DIR, m_last_directory).toString();
		auto filename = QFileDialog::getOpenFileName(this,
			tr("Open Session"),
			lastdir,
			tr("Session Files (%1)").arg(FileQueue::sessionExtensionFilter));
		m_tagger.openSession(filename);
	});
	connect(&a_go_to_number, &QAction::triggered, this, [this]()
	{
		auto number = QInputDialog::getInt(this,
			tr("Enter Number"),
			tr("Enter file number to open:"),
			static_cast<int>(m_tagger.queue().currentIndex()+1), // value
			1,                                                   // min
			static_cast<int>(m_tagger.queue().size()));          // max
		m_tagger.openFileInQueue(number-1);
	});
	connect(&m_notification_display_timer, &QTimer::timeout, this, [this]()
	{
		static bool which = false;
		auto title = which ? tr("Notifications  ") : tr("Notifications: %1").arg(m_notification_count);
		this->menu_notifications.setTitle(title);
		which = !which;
	});
	connect(&m_short_notification_display_timer, &QTimer::timeout, this, [this](){
		this->menu_short_notification.setTitle(QString{});
	});
	connect(&m_tagger, &Tagger::sessionOpened, [](const QString& path)
	{
		QSettings settings; settings.setValue(SETT_LAST_SESSION_DIR, QFileInfo(path).absolutePath());
	});
	connect(&m_tagger, &Tagger::fileOpened, this, [this](const QString& path)
	{
		m_last_directory = QFileInfo(path).absolutePath();
		QSettings settings; settings.setValue(SETT_LAST_DIR, m_last_directory);
	});
	connect(&m_tagger, &Tagger::newTagsAdded, this, [this](const QStringList& l)
	{
		QString msg = tr("<p>New tags were found, ordered by number of times used:</p>");
		msg.append(QStringLiteral("<ul><li>"));
		for(const auto& e : qAsConst(l)) {
			msg.append(e);
			msg.append(QStringLiteral("</li><li>"));
		}
		msg.append(QStringLiteral("</li></ul>"));
		addNotification(tr("New tags added"), tr("Check Notifications menu for list of added tags."), msg);
	});
	connect(&m_tagger, &Tagger::tagFilesNotFound, this, [this](QString normalname, QString overridename, QStringList paths)
	{
		addNotification(tr("Tag file not found"),
		                tr("Could not locate suitable tag file"),
		                tr("<h2>Could not locate suitable tag file</h2>"
		                   "<p>You can still browse and rename files, but tag autocomplete will not work.</p>"
		                   "<hr>WiseTagger will look for <em>tag files</em> in directory of the currently opened file "
		                   "and in directories directly above it."

		                   "<p>Tag files we looked for:"
		                   "<dd><dl>Appending tag file: <b>%1</b></dl>"
		                   "<dl>Overriding tag file: <b>%2</b></dl></dd></p>"
		                   "<p>Directories where we looked for them, in search order:"
		                   "<ol>%3</ol></p>"
		                   "<p><a href=\"https://github.com/0xb8/WiseTagger#tag-file-selection\">"
		                   "Appending and overriding tag files documentation"
		                   "</a></p>").arg(normalname, overridename, util::html_list_join(paths)));
	});
	connect(&m_tagger, &Tagger::tagFilesConflict, this, [this](QString, QString overridename, QStringList paths)
	{

		auto normal_pos = std::find_if_not(paths.begin(), paths.end(), [&overridename](const auto& p) {
			return QDir::match(overridename, QFileInfo{p}.fileName());
		});
		Q_ASSERT(normal_pos != paths.end());

		QStringList overrides{paths.begin(), normal_pos};
		QStringList normals{normal_pos, paths.end()};

		addNotification(tr("Tag files conflict"),
		                tr("Some of the tag files are in conflict"),
		                tr("<p>Because <em>overriding tag files</em> halt the search, the following <em>appending tag files</em> are always ignored:</p>"
		                   "<p><ol>%1</ol></p>"
		                   "<p>Overriding tag files that caused the conflict:</p>"
		                   "<p><ol>%2</ol></p>"
		                   "<p><a href=\"https://github.com/0xb8/WiseTagger#tag-file-selection\">"
		                   "Appending and overriding tag files documentation"
		                   "</a></p>").arg(util::html_list_join(normals), util::html_list_join(overrides)));
	});
	connect(&m_tagger, &Tagger::parseError, this, [this](QString regex_source, QString error, int column)
	{
		auto arrow_left = QStringLiteral("~~~~ ^");
		auto arrow_right = QStringLiteral("^ ~~~~");

		QString arrow;
		if (column < arrow_left.size()) {
			arrow.fill(' ', column);
			arrow.append(arrow_right);
		} else {
			arrow.fill(' ', column - arrow_left.size());
			arrow.append(arrow_left);
		}

		addNotification(tr("Tag file syntax error"), tr("Regular expression syntax error"),
		                tr("<h2>Regular expression syntax error</h2>"
		                   "<p>At column %1: %2</p>"
		                   "<pre style=\"font-family: Consolas, \"Lucida Console\", Monaco,monospace,monospace;\">"
		                   "%3\n\n"
		                   "<span style=\"color: red\">%4</span>"
		                   "<pre>").arg(column)
		                           .arg(error.toHtmlEscaped(),
		                                regex_source.toHtmlEscaped(),
		                                arrow));
	});
	auto show_network_error_notification = [this](QUrl url, QString error)
	{
		auto error_str = tr("Error connecting to %1: %2").arg(url.host(), error);
		addNotification(tr("Network error"), error_str);
		statusBar()->showMessage(tr("Network error: %1").arg(error_str), 3000);
		hideUploadProgress();
	};
	connect(&m_tagger.tag_fetcher(), &TagFetcher::net_error, this, show_network_error_notification);
	connect(&m_reverse_search, &ReverseSearch::error, this, show_network_error_notification);
	connect(&a_show_settings, &QAction::triggered, this, [this]()
	{
		auto sd = new SettingsDialog(this);
		connect(sd, &SettingsDialog::updated, this, &Window::updateSettings);
		connect(sd, &SettingsDialog::updated, this, &Window::updateProxySettings);
		connect(sd, &SettingsDialog::updated, &m_tagger, &Tagger::updateSettings);
		connect(sd, &SettingsDialog::restart, this, [this](){
			pdbg << "scheduling restart...";
			QTimer::singleShot(100, this, &Window::scheduleRestart);
		});
		connect(sd, &SettingsDialog::finished, [sd](int){
			sd->deleteLater();
		});
		sd->open();
	});
	connect(&ag_sort_criteria, &QActionGroup::triggered, this, [this](QAction* a)
	{
		auto criteria = a->data().value<SortQueueBy>();
		m_tagger.queue().setSortBy(criteria);
		m_tagger.queue().sort();

		QString criteria_str;
		switch (criteria) {
		case SortQueueBy::FileName:
			criteria_str = tr("Name");
			break;
		case SortQueueBy::FileType:
			criteria_str = tr("Type");
			break;
		case SortQueueBy::ModificationDate:
			criteria_str = tr("Modification Date");
			break;
		case SortQueueBy::FileSize:
			criteria_str = tr("Size");
			break;
		case SortQueueBy::FileNameLength:
			criteria_str = tr("Name Length");
			break;
		case SortQueueBy::TagCount:
			criteria_str = tr("Tag Count");
		}

		addNotification(tr("Queue Sorted by %1").arg(criteria_str));
	});
	connect(&a_fetch_tags,  &QAction::triggered, &m_tagger, &Tagger::fetchTags);
	connect(&m_tagger.tag_fetcher(), &TagFetcher::ready, this, [this](QString, QString)
	{
		hideUploadProgress();
		alert();
		statusBar()->showMessage(tr("Tag Fetching Done."), 3000);
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
	add_action(menu_file, a_open_dir_recurse);
	add_action(menu_file, a_open_session);
	add_separator(menu_file);
	add_action(menu_file, a_save_file);
	add_action(menu_file, a_save_session);
	add_separator(menu_file);
	add_action(menu_file, a_fix_tags);
	add_action(menu_file, a_copy_file);
	add_action(menu_file, a_delete_file);
	add_separator(menu_file);
	add_action(menu_file, a_open_post);
	add_action(menu_file, a_fetch_tags);
	add_action(menu_file, a_iqdb_search);
	add_separator(menu_file);
	add_action(menu_file, a_hide);
	add_action(menu_file, a_exit);

	// Tray context menu actions
	add_action(menu_tray, a_view_normal);
	add_action(menu_tray, a_view_fullscreen);
	add_action(menu_tray, a_view_minimal);
	add_separator(menu_tray);
	add_action(menu_tray, a_view_menu);
	add_action(menu_tray, a_view_input);
	add_action(menu_tray, a_view_statusbar);
	add_separator(menu_tray);
	add_action(menu_tray, a_edit_mode);
	add_action(menu_tray, a_ib_replace);
	add_action(menu_tray, a_ib_restore);
	add_action(menu_tray, a_tag_forcefirst);
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
	add_action(menu_navigation, a_next_fixable);
	add_action(menu_navigation, a_prev_fixable);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_set_queue_filter);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_open_loc);
	add_action(menu_navigation, a_open_tags);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_reload_tags);
	add_action(menu_navigation, a_edit_tags);
	add_action(menu_navigation, a_edit_temp_tags);

	// View menu actions
	add_action(menu_view, a_view_fullscreen);
	add_action(menu_view, a_view_slideshow);
	add_action(menu_view, a_view_minimal);
	add_separator(menu_view);
	add_action(menu_view, a_view_menu);
	add_action(menu_view, a_view_input);
	add_action(menu_view, a_view_statusbar);
	add_separator(menu_view);
	menu_view.addMenu(&menu_sort);

	// Play menu actions
	add_action(menu_play, a_play_pause);
	add_action(menu_play, a_play_mute);

	// Sort menu actions
	add_action(menu_sort, a_view_sort_name);
	add_action(menu_sort, a_view_sort_type);
	add_action(menu_sort, a_view_sort_size);
	add_action(menu_sort, a_view_sort_date);
	add_action(menu_sort, a_view_sort_length);
	add_action(menu_sort, a_view_sort_tagcnt);
	a_view_sort_name.setCheckable(true);
	a_view_sort_name.setChecked(true);
	a_view_sort_type.setCheckable(true);
	a_view_sort_size.setCheckable(true);
	a_view_sort_date.setCheckable(true);
	a_view_sort_length.setCheckable(true);
	a_view_sort_tagcnt.setCheckable(true);
	a_view_sort_name.setActionGroup(&ag_sort_criteria);
	a_view_sort_type.setActionGroup(&ag_sort_criteria);
	a_view_sort_size.setActionGroup(&ag_sort_criteria);
	a_view_sort_date.setActionGroup(&ag_sort_criteria);
	a_view_sort_length.setActionGroup(&ag_sort_criteria);
	a_view_sort_tagcnt.setActionGroup(&ag_sort_criteria);
	a_view_sort_name.setData(QVariant::fromValue(SortQueueBy::FileName));
	a_view_sort_type.setData(QVariant::fromValue(SortQueueBy::FileType));
	a_view_sort_size.setData(QVariant::fromValue(SortQueueBy::FileSize));
	a_view_sort_date.setData(QVariant::fromValue(SortQueueBy::ModificationDate));
	a_view_sort_length.setData(QVariant::fromValue(SortQueueBy::FileNameLength));
	a_view_sort_tagcnt.setData(QVariant::fromValue(SortQueueBy::TagCount));

	// Options menu actions
	add_action(menu_options, a_edit_mode);
	add_separator(menu_options);
	add_action(menu_options, a_rotate_cw);
	add_action(menu_options, a_rotate_ccw);
	add_action(menu_options, a_fit_to_screen);
	add_separator(menu_options);
	add_action(menu_options, a_navigate_by_wheel);
	add_separator(menu_options);
	add_action(menu_options, a_ib_replace);
	add_action(menu_options, a_ib_restore);
	add_action(menu_options, a_tag_forcefirst);
	add_separator(menu_options);
	add_action(menu_options, a_show_settings);

	// Help menu actions
	add_action(menu_help, a_help);
	add_action(menu_help, a_stats);
	add_separator(menu_help);
	add_action(menu_help, a_about);
	add_action(menu_help, a_about_qt);

	// Tagger context menu
	add_action(menu_context_tagger, a_exit_slideshow);
	add_action(menu_context_tagger, a_exit_fullscreen);
	add_separator(menu_context_tagger);
	add_action(menu_context_tagger, a_next_file);
	add_action(menu_context_tagger, a_prev_file);
	add_action(menu_context_tagger, a_navigate_by_wheel);
	add_separator(menu_context_tagger);
	menu_context_tagger.addMenu(&menu_view);
	menu_context_tagger.addMenu(&menu_sort);
	menu_context_tagger.addMenu(&menu_context_commands);
	add_action(menu_context_tagger, a_set_queue_filter);
	add_separator(menu_context_tagger);
	add_action(menu_context_tagger, a_ib_replace);
	add_action(menu_context_tagger, a_ib_restore);
	add_action(menu_context_tagger, a_tag_forcefirst);
	add_separator(menu_context_tagger);
	add_action(menu_context_tagger, a_show_settings);
	add_action(menu_context_tagger, a_hide);
	add_action(menu_context_tagger, a_exit);

	// Menu bar menus
	menuBar()->addMenu(&menu_file);
	menuBar()->addMenu(&menu_navigation);
	menuBar()->addMenu(&menu_view);
	a_menu_commands_action = menuBar()->addMenu(&menu_commands);
	menu_commands.setToolTipsVisible(true);
	menuBar()->addMenu(&menu_options);
	menuBar()->addMenu(&menu_help);
	menuBar()->addSeparator();
	menuBar()->addMenu(&menu_short_notification);
	menuBar()->addMenu(&menu_notifications);

	// Tray context menu
	m_tray_icon.setContextMenu(&menu_tray);

	// tagger context menu
	m_tagger.setContextMenuPolicy(Qt::CustomContextMenu);
	connect(&m_tagger, &Tagger::customContextMenuRequested, this, [this](const QPoint& pos) {
		auto widget = qobject_cast<QWidget*>(sender());
		a_exit_slideshow.setVisible(a_view_slideshow.isChecked());
		a_exit_fullscreen.setVisible(!a_view_slideshow.isChecked() && a_view_fullscreen.isChecked());
		menu_context_tagger.exec(widget->mapToGlobal(pos));
	});

	// Status bar items
	setStatusBar(&m_statusbar);
	m_statusbar.setObjectName(QStringLiteral("StatusBar"));
	m_statusbar_label.setObjectName(QStringLiteral("StatusbarInfo"));
	m_statusbar_label.setOpenExternalLinks(false);
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
	a_next_fixable.setDisabled(val);
	a_prev_fixable.setDisabled(val);
	a_delete_file.setDisabled(val);
	a_copy_file.setDisabled(val);
	a_fetch_tags.setDisabled(val || !m_tagger.fileRenameable());
	a_open_post.setDisabled(m_post_url.isEmpty());
	a_iqdb_search.setDisabled(val);
	a_open_loc.setDisabled(val);
	a_save_session.setDisabled(val);
	a_go_to_number.setDisabled(val);
	a_edit_temp_tags.setDisabled(val);
	menu_commands.setDisabled(val);
	menu_context_commands.setDisabled(val);

	bool is_playable = m_tagger.mediaIsVideo() || m_tagger.mediaIsAnimatedImage();
	if (is_playable) {
		// create play menu if needed
		if (!a_menu_play_action) {
			a_menu_play_action = menuBar()->insertMenu(a_menu_commands_action, &menu_play);
		}
		// video plays by default on new file open
		a_play_pause.setChecked(m_tagger.mediaIsPlaying());
	} else {
		// remove play menu if needed
		if (a_menu_play_action) {
			menuBar()->removeAction(a_menu_play_action);
			a_menu_play_action = nullptr;
		}
	}

	a_play_pause.setEnabled(!val && is_playable);
	a_play_mute.setEnabled(!val && m_tagger.mediaIsVideo());

	a_fit_to_screen.setDisabled(m_tagger.mediaIsVideo());
	a_rotate_cw.setDisabled(m_tagger.mediaIsVideo() || m_tagger.mediaIsAnimatedImage() || val);
	a_rotate_ccw.setEnabled(a_rotate_cw.isEnabled());

	val = m_tagger.hasTagFile();
	a_reload_tags.setEnabled(val);
	a_open_tags.setEnabled(val);
	a_edit_tags.setEnabled(val);

	val = m_tagger.editMode() == EditMode::Naming;
	a_ib_replace.setDisabled(val);
	a_ib_restore.setDisabled(val);
	a_tag_forcefirst.setDisabled(val);
	a_fix_tags.setDisabled(val || m_tagger.isEmpty());
}

//------------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(nullptr,
	tr("About %1").arg(QStringLiteral(TARGET_PRODUCT)),
	util::read_resource_html("about.html")
		.arg(QStringLiteral(TARGET_PRODUCT), qApp->applicationVersion())
		.arg(QDate::currentDate().year()));
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
