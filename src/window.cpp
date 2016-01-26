/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QCommandLineParser>
#include <QDesktopServices>
#include <QDirIterator>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QMimeData>
#include <QSettings>
#include <QCollator>

#include <QKeySequence>
#include <QMessageBox>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include <QMenuBar>

#include "window.h"
#include "util/open_graphical_shell.h"
#include "util/debug.h"
#include "util/size.h"

Window::Window(QWidget *_parent) :
	QMainWindow(_parent)
	, m_tagger(this)
	, m_reverse_search(this)
	, a_open_file(	tr("&Open File..."), nullptr)
	, a_open_dir(	tr("Open &Folder..."), nullptr)
	, a_delete_file(tr("&Delete Current Image"), nullptr)
	, a_open_post(	tr("Open Imageboard &Post..."), nullptr)
	, a_iqdb_search(tr("&Reverse Search Image..."), nullptr)
	, a_exit(	tr("&Exit"), nullptr)
	, a_next_file(	tr("&Next Image"), nullptr)
	, a_prev_file(	tr("&Previous Image"), nullptr)
	, a_save_file(	tr("&Save"), nullptr)
	, a_save_next(	tr("Save and Open Next Image"), nullptr)
	, a_save_prev(	tr("Save and Open Previous Image"), nullptr)
	, a_open_loc(	tr("Open &Containing Folder"), nullptr)
	, a_reload_tags(tr("&Reload Tag File"), nullptr)
	, a_ib_replace(	tr("Re&place Imageboard Tags"), nullptr)
	, a_ib_restore(	tr("Re&store Imageboard Tags"), nullptr)
	, a_toggle_statusbar(tr("&Toggle Statusbar"), nullptr)
	, a_about(	tr("&About..."), nullptr)
	, a_about_qt(	tr("About &Qt..."), nullptr)
	, a_help(	tr("&Help..."), nullptr)
	, menu_file(	tr("&File"))
	, menu_navigation(tr("&Navigation"))
	, menu_options(	tr("&Options"))
	, menu_help(	tr("&Help"))
	, m_statusbar(nullptr)
	, m_statusbar_info_label(nullptr)
{
	setCentralWidget(&m_tagger);
	setAcceptDrops(true);

	createActions();
	createMenus();

	loadWindowSettings();
	loadWindowStyles();
	parseCommandLineArguments();
}

Window::~Window() { }

//------------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	QString fileName = QFileDialog::getOpenFileName(this,
		tr("Open File"),
		m_last_directory,
		tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png)"));

	m_tagger.openFile(fileName);
}

void Window::directoryOpenDialog()
{
	QString dir = QFileDialog::getExistingDirectory(nullptr,
		tr("Open Directory"),
		m_last_directory,
		QFileDialog::ShowDirsOnly);

	m_tagger.openDir(dir);
}


void Window::updateWindowTitle()
{
	if(m_tagger.queue().empty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	m_last_directory = m_tagger.currentDir();
	setWindowTitle(tr(Window::MainWindowTitle)
		.arg(m_tagger.currentFileName())
		.arg(m_tagger.fileModified() ? QChar('*') : QStringLiteral(""))
		.arg(m_tagger.pictureDimensions().width())
		.arg(m_tagger.pictureDimensions().height())
		.arg(util::size::printable(m_tagger.pictureSize()))
		.arg(qApp->applicationVersion()));
}

void Window::updateWindowTitleProgress(int progress)
{
	if(m_tagger.queue().empty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	setWindowTitle(tr(Window::MainWindowTitleProgress)
		.arg(m_tagger.currentFileName())
		.arg(m_tagger.fileModified() ? QChar('*') : QStringLiteral(""))
		.arg(m_tagger.pictureDimensions().width())
		.arg(m_tagger.pictureDimensions().height())
		.arg(util::size::printable(m_tagger.pictureSize()))
		.arg(qApp->applicationVersion())
		.arg(progress));
}

void Window::updateStatusBarText()
{
	if(m_tagger.queue().empty()) {
		m_statusbar_info_label.clear();
		return;
	}
	auto current = m_tagger.queue().currentIndex() + 1u;
	auto size    = m_tagger.queue().size();
	m_statusbar_info_label.setText(tr("%1 / %2  ").arg(current).arg(size));
}

void Window::updateImageboardPostURL(QString url)
{
	a_open_post.setDisabled(url.isEmpty());
	if(url.isEmpty()) {
		m_post_url.clear();
	} else {
		m_post_url = url;
	}

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
		tr("Uploading <b>%1</b> to iqdb.org...  %2% complete")
			.arg(m_tagger.currentFileName())
			.arg(util::size::percent(bytesSent, bytesTotal)));
}

void Window::hideUploadProgress()
{
#ifdef Q_OS_WIN32
	m_win_taskbar_button.progress()->setVisible(false);
#endif
	updateWindowTitle();
	statusBar()->showMessage(tr("Done."), 3000);
}

void Window::updateCurrentDirectory()
{
	m_last_directory = m_tagger.currentDir();
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
			if(!(dropfile.isDir() || m_tagger.queue().checkExtension(dropfile.suffix()))) {
				return true;
			}
		}

		drag_event->acceptProposedAction();
		return true;
	}
	if(e->type() == QEvent::Drop) {
		auto drop_event = static_cast<QDropEvent*>(e);
		const auto fileurls = drop_event->mimeData()->urls();
		QFileInfo dropfile;

		Q_ASSERT(!fileurls.empty());

		if(fileurls.size() == 1) {
			dropfile.setFile(fileurls.first().toLocalFile());

			if(dropfile.isFile())
				m_tagger.openFile(dropfile.absoluteFilePath());

			if(dropfile.isDir())
				m_tagger.openDir(dropfile.absoluteFilePath());

			return true;
		}

		m_tagger.queue().clear();
		for(auto&& fileurl : fileurls) {
			dropfile.setFile(fileurl.toLocalFile());

			if(dropfile.isFile() && m_tagger.queue().checkExtension(dropfile.suffix())) {
				m_tagger.queue().push(dropfile.absoluteFilePath());
				pdbg << "added file" << dropfile.absoluteFilePath();
			}

			if(dropfile.isDir()) {
				m_tagger.queue().push(dropfile.absoluteFilePath());
				pdbg << "added dir" << dropfile.absoluteFilePath();
			}
		}

		pdbg << "loaded" << m_tagger.queue().size() << "images from" << fileurls.size() << "dropped items";

		m_tagger.queue().sort();
		m_tagger.queue().select(0u);
		m_tagger.loadCurrentFile();
		return true;
	}
	return false;
}

void Window::closeEvent(QCloseEvent *e)
{
	if(m_tagger.fileModified()) {
		auto r = m_tagger.rename();
		if(r == RenameStatus::Renamed || r == RenameStatus::Failed) {
			saveWindowSettings();
			e->accept();
			return;
		}
		e->ignore();
		return;
	}
	saveWindowSettings();
	e->accept();
}

void Window::showEvent(QShowEvent *e)
{
#ifdef Q_OS_WIN32
	// NOTE: workaround for taskbar button not working
	m_win_taskbar_button.setWindow(this->windowHandle());
#endif
	e->accept();
}

//------------------------------------------------------------------------------

void Window::parseCommandLineArguments()
{
	QFileInfo f;
	QStringList args = qApp->arguments();

	QCommandLineParser parser;
	parser.addVersionOption();
	parser.addHelpOption();
	QCommandLineOption proxy_option(QStringLiteral("proxy"),
					QStringLiteral("Set proxy for IQDB search"),
					QStringLiteral("proxy"));
	QCommandLineOption no_proxy_option(QStringLiteral("no-proxy"),
					   QStringLiteral("Disable proxy"));
	parser.addOption(proxy_option);
	parser.addOption(no_proxy_option);
	parser.process(args);

	if(parser.isSet(proxy_option)) {
		QString proxy_arg = parser.value(QStringLiteral("proxy"));
		pdbg << "proxy arg" << proxy_arg;
		m_reverse_search.setProxy({proxy_arg, QUrl::StrictMode});
	}

	if(parser.isSet(no_proxy_option)) {
		pdbg << "--no-proxy";
		m_reverse_search.setProxyEnabled(false);
	}

	for(const auto& arg : parser.positionalArguments()) {
		if(arg.startsWith(QChar('-'))) {
			continue;
		}
		f.setFile(arg);

		if(!f.exists()) {
			continue;
		}

		if(f.isFile()) {
			m_tagger.queue().push(f.absoluteFilePath());
		}

		if(f.isDir()) {
			m_tagger.queue().push(f.absoluteFilePath());
		}
	}
	if(m_tagger.queue().size() == 1) {
		m_tagger.openFile(m_tagger.queue().select(0u));
	}
	else {
		m_tagger.queue().sort();
		m_tagger.queue().select(0u);
		m_tagger.loadCurrentFile();
	}
}

//------------------------------------------------------------------------------

void Window::saveWindowSettings()
{
	QSettings settings;
	settings.beginGroup(QStringLiteral("window"));
	settings.setValue(QStringLiteral("size"), this->size());
	settings.setValue(QStringLiteral("position"), this->pos());
	settings.setValue(QStringLiteral("maximized"), this->isMaximized());
	settings.setValue(QStringLiteral("last-directory"), m_last_directory);
	settings.endGroup();
}

void Window::loadWindowStyles()
{
	QFile styles_file(QStringLiteral(":/css/style.default.css"));
	auto open = styles_file.open(QIODevice::ReadOnly);
	Q_ASSERT(open);
	qApp->setStyleSheet(styles_file.readAll());
}

void Window::loadWindowSettings()
{
	// check if running in portable mode
	if(QFile(qApp->applicationDirPath() +
			QStringLiteral("/portable.dat")).exists())
	{
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			qApp->applicationDirPath()+QStringLiteral("/settings"));
	}

	QSettings settings;
	settings.beginGroup(QStringLiteral("window"));
	m_last_directory = settings.value(QStringLiteral("last-directory")).toString();
	resize(settings.value(QStringLiteral("size"), QSize(1024,600)).toSize());

	if(settings.contains(QStringLiteral("position")))
		move(settings.value(QStringLiteral("position")).toPoint());

	if(settings.value(QStringLiteral("maximized"), false).toBool())
		showMaximized();

	a_toggle_statusbar.setChecked(settings.value(
		QStringLiteral("show-statusbar"), false).toBool());

	m_statusbar.setVisible(settings.value(
		QStringLiteral("show-statusbar"), false).toBool());

	settings.endGroup();

	a_ib_replace.setChecked(settings.value(
		QStringLiteral("imageboard/replace-tags"), false).toBool());

	a_ib_restore.setChecked(settings.value(
		QStringLiteral("imageboard/restore-tags"), true).toBool());


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
	a_reload_tags.setShortcut(  QKeySequence(tr("Ctrl+R", "Reload Tags")));
	a_open_post.setShortcut(    QKeySequence(tr("Ctrl+P", "Open post")));
	a_iqdb_search.setShortcut(  QKeySequence(tr("Ctrl+F", "Reverse search")));
	a_open_loc.setShortcut(	    QKeySequence(tr("Ctrl+L", "Open file location")));
	a_help.setShortcut(         QKeySequence::HelpContents);
	a_exit.setShortcut(         QKeySequence::Close);

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

	a_ib_replace.setCheckable(true);
	a_ib_restore.setCheckable(true);
	a_toggle_statusbar.setCheckable(true);

	connect(&m_tagger,         &Tagger::postURLChanged,        this, &Window::updateImageboardPostURL);
	connect(&m_reverse_search, &ReverseSearch::uploadProgress, this, &Window::showUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, &Window::hideUploadProgress);

	connect(&a_open_file,   &QAction::triggered, this, &Window::fileOpenDialog);
	connect(&a_open_dir,    &QAction::triggered, this, &Window::directoryOpenDialog);
	connect(&a_exit,        &QAction::triggered, this, &Window::close);
	connect(&a_about,       &QAction::triggered, this, &Window::about);
	connect(&a_about_qt,    &QAction::triggered, qApp, &QApplication::aboutQt);
	connect(&a_help,        &QAction::triggered, this, &Window::help);
	connect(&a_delete_file, &QAction::triggered, &m_tagger, &Tagger::deleteCurrentFile);
	connect(&a_reload_tags, &QAction::triggered, &m_tagger, &Tagger::reloadTags);
	connect(&m_tagger,      &Tagger::tagsEdited, this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateMenus);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateWindowTitle);
	connect(&m_tagger,      &Tagger::fileOpened, this, &Window::updateStatusBarText);

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
		m_tagger.nextFile(RenameFlags::Force);
	});
	connect(&a_save_prev,   &QAction::triggered, [this]()
	{
		m_tagger.prevFile(RenameFlags::Force);
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
		QSettings settings;
		settings.setValue(QStringLiteral("imageboard/replace-tags"), checked);
	});
	connect(&a_ib_restore,  &QAction::triggered, [](bool checked)
	{
		QSettings settings;
		settings.setValue(QStringLiteral("imageboard/restore-tags"), checked);
	});
	connect(&a_toggle_statusbar, &QAction::triggered, [this](bool checked)
	{
		QSettings settings;
		settings.setValue(QStringLiteral("window/show-statusbar"), checked);
		m_statusbar.setVisible(checked);
	});
}

void Window::createMenus()
{
	menu_file.addAction(&a_open_file);
	menu_file.addAction(&a_open_dir);
	menu_file.addSeparator();
	menu_file.addAction(&a_save_file);
	menu_file.addSeparator();
	menu_file.addAction(&a_delete_file);

	menu_file.addSeparator();
	menu_file.addAction(&a_open_post);
	menu_file.addAction(&a_iqdb_search);
	menu_file.addSeparator();
	menu_file.addAction(&a_exit);

	menu_navigation.addAction(&a_next_file);
	menu_navigation.addAction(&a_prev_file);
	menu_navigation.addSeparator();
	menu_navigation.addAction(&a_save_next);
	menu_navigation.addAction(&a_save_prev);
	menu_navigation.addSeparator();
	menu_navigation.addAction(&a_open_loc);
	menu_navigation.addAction(&a_reload_tags);

	menu_options.addAction(&a_ib_replace);
	menu_options.addAction(&a_ib_restore);
	menu_options.addSeparator();
	menu_options.addAction(&a_toggle_statusbar);

	menu_help.addAction(&a_help);
	menu_help.addSeparator();
	menu_help.addAction(&a_about);
	menu_help.addAction(&a_about_qt);

	menuBar()->addMenu(&menu_file);
	menuBar()->addMenu(&menu_navigation);
	menuBar()->addMenu(&menu_options);
	menuBar()->addMenu(&menu_help);

	setStatusBar(&m_statusbar);
	m_statusbar.setObjectName(QStringLiteral("StatusBar"));
	m_statusbar_info_label.setObjectName(QStringLiteral("StatusbarInfo"));
	m_statusbar.setSizeGripEnabled(false);
	m_statusbar.addPermanentWidget(&m_statusbar_info_label);
}

void Window::updateMenus()
{
	bool val = m_tagger.queue().empty();
	a_next_file.setDisabled(val);
	a_prev_file.setDisabled(val);
	a_save_file.setDisabled(val);
	a_save_next.setDisabled(val);
	a_save_prev.setDisabled(val);
	a_delete_file.setDisabled(val);
	a_reload_tags.setDisabled(val);
	a_open_post.setDisabled(m_post_url.isEmpty());
	a_iqdb_search.setDisabled(val);
	a_open_loc.setDisabled(val);
}

//------------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(nullptr,
	tr("About WiseTagger"),
	tr("<h3>WiseTagger v%1</h3><p>Built %2, %3.</p><p>Copyright &copy; 2015 catgirl &lt;"
	   "<a href=\"mailto:cat@wolfgirl.org\">cat@wolfgirl.org</a>&gt; (bugreports are very welcome!)</p>"
	   "<p>This program is free software. It comes without any warranty, to the extent permitted by applicable law. "
	   "You can redistribute it and/or modify it under the terms of the Do What The Fuck You Want To Public License, "
	   "Version 2, as published by Sam Hocevar. See <a href=\"http://www.wtfpl.net\">http://www.wtfpl.net/</a> "
	   "for more details.</p>"
	).arg(qApp->applicationVersion(), QStringLiteral(__DATE__), QStringLiteral(__TIME__)));
}

void Window::help()
{
	QMessageBox::about(nullptr,
	tr("Help"),
	tr("<h2>User Interface</h2>"
	   "<p><b>Tab</b> &ndash; list autocomplete suggestions</p>"
	   "<p><b>Enter</b> &ndash; apply changes and clear focus</p>"
	   "<p><b>Left</b> and <b>Right</b> arrows &ndash; show previous/next picture</p>"
	   "<p>More documentation at <a href=\"https://bitbucket.org/catgirl/wisetagger\">project repository page</a>.</p>"
	));
}

//------------------------------------------------------------------------
