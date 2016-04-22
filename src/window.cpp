/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QApplication>
#include <QCollator>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QUrl>

#include "window.h"
#include "util/open_graphical_shell.h"
#include "util/size.h"
#include "util/misc.h"

Q_LOGGING_CATEGORY(wilc, "Window")
#define pdbg qCDebug(wilc)
#define pwarn qCWarning(wilc)

#define SETT_WINDOW_SIZE	QStringLiteral("window/size")
#define SETT_WINDOW_POS		QStringLiteral("window/position")
#define SETT_FULLSCREEN		QStringLiteral("window/show-fullscreen")
#define SETT_MAXIMIZED		QStringLiteral("window/maximized")
#define SETT_LAST_DIR		QStringLiteral("window/last-directory")

#define SETT_SHOW_MENU		QStringLiteral("window/show-menu")
#define SETT_SHOW_STATUS	QStringLiteral("window/show-statusbar")
#define SETT_SHOW_INPUT		QStringLiteral("window/show-input")
#define SETT_LOCALE		QStringLiteral("window/locale")
#define SETT_STYLE		QStringLiteral("window/style")

#define SETT_REPLACE_TAGS	QStringLiteral("imageboard/replace-tags")
#define SETT_RESTORE_TAGS	QStringLiteral("imageboard/restore-tags")

#define SESSION_FILE_SUFFIX	QStringLiteral("wt-session")
#define SESSION_FILE_PATTERN	QStringLiteral("*.wt-session")

//------------------------------------------------------------------------------

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
	, a_save_session(tr("Save Session"), nullptr)
	, a_load_session(tr("Open Session"),nullptr)
	, a_open_loc(	tr("Open &Containing Folder"), nullptr)
	, a_reload_tags(tr("&Reload Tag File"), nullptr)
	, a_ib_replace(	tr("Re&place Imageboard Tags"), nullptr)
	, a_ib_restore(	tr("Re&store Imageboard Tags"), nullptr)
	, a_view_statusbar( tr("&Statusbar"), nullptr)
	, a_view_fullscreen(tr("&Fullscreen"), nullptr)
	, a_view_menu(      tr("&Menu"), nullptr)
	, a_view_input(     tr("Tag &Input"), nullptr)
	, a_about(	tr("&About..."), nullptr)
	, a_about_qt(	tr("About &Qt..."), nullptr)
	, a_help(	tr("&Help..."), nullptr)
	, a_stats(	tr("&Statistics..."), nullptr)
	, menu_file(	tr("&File"))
	, menu_navigation(tr("&Navigation"))
	, menu_view(    tr("&View"), nullptr)
	, menu_options(	tr("&Options"))
	, menu_options_language(tr("&Language"))
	, menu_options_style(tr("S&tyle"))
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

//------------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	auto fileName = QFileDialog::getOpenFileName(this,
		tr("Open File"),
		m_last_directory,
		tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png *.bmp)"));

	m_tagger.openFile(fileName);
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
	if(m_tagger.queue().empty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	m_last_directory = m_tagger.currentDir();
	setWindowTitle(tr(Window::MainWindowTitle).arg(
		m_tagger.currentFileName(),
		m_tagger.fileModified() ? QStringLiteral("*") : QStringLiteral(""),
		QString::number(m_tagger.pictureDimensions().width()),
		QString::number(m_tagger.pictureDimensions().height()),
		util::size::printable(m_tagger.pictureSize()),
		qApp->applicationVersion()));
}

void Window::updateWindowTitleProgress(int progress)
{
	if(m_tagger.queue().empty()) {
		setWindowTitle(tr(Window::MainWindowTitleEmpty)
			.arg(qApp->applicationVersion()));
		return;
	}
	setWindowTitle(tr(Window::MainWindowTitleProgress).arg(
		m_tagger.currentFileName(),
		m_tagger.fileModified() ? QStringLiteral("*") : QStringLiteral(""),
		QString::number(m_tagger.pictureDimensions().width()),
		QString::number(m_tagger.pictureDimensions().height()),
		util::size::printable(m_tagger.pictureSize()),
		qApp->applicationVersion(),
		QString::number(progress)));
}

void Window::updateStatusBarText()
{
	if(m_tagger.queue().empty()) {
		m_statusbar_info_label.clear();
		return;
	}
	auto current = m_tagger.queue().currentIndex() + 1u;
	auto qsize    = m_tagger.queue().size();
	m_statusbar_info_label.setText(QStringLiteral("%1 / %2  ")
		.arg(QString::number(current),QString::number(qsize)));
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
			if(!(dropfile.isDir()
				|| m_tagger.queue().checkExtension(dropfile.suffix())
				|| dropfile.suffix() == SESSION_FILE_SUFFIX))
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

			if(dropfile.isFile()) {
				if(dropfile.suffix() == SESSION_FILE_SUFFIX) {
					m_tagger.openSession(dropfile.absoluteFilePath());
				} else {
					m_tagger.openFile(dropfile.absoluteFilePath());
				}
			}

			if(dropfile.isDir())
				m_tagger.openDir(dropfile.absoluteFilePath());

			return true;
		}

		m_tagger.queue().clear();
		for(auto&& fileurl : fileurls) {
			dropfile.setFile(fileurl.toLocalFile());
			if(dropfile.isFile() || dropfile.isDir()) {
				m_tagger.queue().push(dropfile.absoluteFilePath());
				pdbg << "added" << dropfile.absoluteFilePath();
			}
		}

		pdbg << "loaded" << m_tagger.queue().size() << "images from" << fileurls.size() << "dropped items";

		m_tagger.openFileInQueue();
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
	QCommandLineOption no_stats_option(QStringLiteral("no-stats"),
					   QStringLiteral("Disable statistics collection"));
	parser.addOption(proxy_option);
	parser.addOption(no_proxy_option);
	parser.addOption(no_stats_option);
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

	if(parser.isSet(no_stats_option)) {
		m_tagger.statistics().setEnabled(false);
	}

	for(const auto& arg : parser.positionalArguments()) {
		if(arg.startsWith(QChar('-'))) {
			continue;
		}
		f.setFile(arg);

		if(!f.exists()) {
			continue;
		}

		if(f.suffix() == SESSION_FILE_SUFFIX) {
			m_tagger.openSession(f.absoluteFilePath());
			return;
		}

		if(f.isFile() || f.isDir()) {
			m_tagger.queue().push(f.absoluteFilePath());
		}
	}
	m_tagger.queue().sort();
	m_tagger.openFileInQueue();
}

//------------------------------------------------------------------------------
void Window::saveWindowSettings()
{
	QSettings settings;
	settings.setValue(SETT_WINDOW_SIZE, this->size());
	settings.setValue(SETT_WINDOW_POS,  this->pos());
	settings.setValue(SETT_MAXIMIZED,   this->isMaximized());
	settings.setValue(SETT_LAST_DIR,    m_last_directory);
}

void Window::loadWindowStyles()
{
	QSettings sett;
	QFile styles_file(sett.value(SETT_STYLE, QStringLiteral(":/css/default.css")).toString());
	bool open = styles_file.open(QIODevice::ReadOnly);
	Q_ASSERT(open);
	qApp->setStyleSheet(styles_file.readAll());
	qApp->setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
}

void Window::loadWindowSettings()
{
	QSettings sett;

	m_last_directory = sett.value(SETT_LAST_DIR).toString();
	resize(sett.value(SETT_WINDOW_SIZE, QSize(1024,600)).toSize());

	if(sett.contains(SETT_WINDOW_POS)) {
		move(sett.value(SETT_WINDOW_POS).toPoint());
	}
	bool maximized   = sett.value(SETT_MAXIMIZED,   false).toBool();
	bool fullscreen  = sett.value(SETT_FULLSCREEN,  false).toBool();
	bool show_status = sett.value(SETT_SHOW_STATUS, false).toBool();
	bool show_menu   = sett.value(SETT_SHOW_MENU,   true).toBool();
	bool show_input  = sett.value(SETT_SHOW_INPUT,  true).toBool();

	if(fullscreen) {
		showFullScreen();
	} else if(maximized) {
		showMaximized();
	} else {
		showNormal();
	}

	menuBar()->setVisible(show_menu);
	m_tagger.setInputVisible(show_input);
	m_statusbar.setVisible(show_status);

	a_view_fullscreen.setChecked(fullscreen);
	a_view_statusbar.setChecked(show_status);
	a_view_menu.setChecked(show_menu);
	a_view_input.setChecked(show_input);


	a_ib_replace.setChecked(sett.value(SETT_REPLACE_TAGS, false).toBool());
	a_ib_restore.setChecked(sett.value(SETT_RESTORE_TAGS, true).toBool());
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
	a_view_fullscreen.setShortcut(QKeySequence::FullScreen);
	a_view_menu.setShortcut(    QKeySequence(Qt::CTRL + Qt::Key_M));
	a_view_input.setShortcut(   QKeySequence(Qt::CTRL + Qt::Key_I));

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
	a_save_session.setEnabled(false);

	a_open_post.setStatusTip(  tr("Open imageboard post of this image."));
	a_iqdb_search.setStatusTip(tr("Upload this image to iqdb.org and open search results page in default browser."));
	a_open_loc.setStatusTip(   tr("Open folder where this image is located."));
	a_reload_tags.setStatusTip(tr("Reload changes in tag files."));
	a_ib_replace.setStatusTip( tr("Toggle replacing certain imageboard tags with their shorter version."));
	a_ib_restore.setStatusTip( tr("Toggle restoring imageboard tags back to their original version."));

	a_ib_replace.setCheckable(true);
	a_ib_restore.setCheckable(true);
	a_view_statusbar.setCheckable(true);
	a_view_fullscreen.setCheckable(true);
	a_view_menu.setCheckable(true);
	a_view_input.setCheckable(true);

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
	connect(&a_stats,       &QAction::triggered, &m_tagger.statistics(), &TaggerStatistics::showStatsDialog);
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
		m_tagger.statistics().reverseSearched();
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
		m_statusbar.setVisible(checked);
	});
	connect(&a_view_fullscreen, &QAction::triggered, [this](bool checked)
	{
		QSettings s; s.setValue(SETT_FULLSCREEN, checked);
		if(checked) {
			this->showFullScreen();
		} else if(s.value(SETT_MAXIMIZED, false).toBool()) {
			this->showMaximized();
		} else {
			this->showNormal();
			this->resize(QSize(1024,600));
		}
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
	connect(&a_save_session, &QAction::triggered, [this](){
		auto filename = QFileDialog::getSaveFileName(this,
			tr("Save Session"),
			m_last_directory,
			QStringLiteral("Session Files (%1)").arg(SESSION_FILE_PATTERN));
		if(filename.isEmpty())
			return;

		QFileInfo fi(filename);
		QString newname = fi.absolutePath();
		newname.append(tr("/%1.%2.%3").arg(
			fi.baseName(),
			QString::number(m_tagger.queue().currentIndex()+1),
			SESSION_FILE_SUFFIX));

		if(fi.exists() && fi.isFile()) {
			QFile::rename(filename,newname);
		}

		if(!m_tagger.queue().saveToFile(newname)) {
			QMessageBox::critical(this,
				tr("Save Session Error"),
				tr("<p>Could not save session to <b>%1</b>.</p><p>Check file permissions.</p>").arg(filename));
		}
	});
	connect(&a_load_session, &QAction::triggered, [this](){
		auto filename = QFileDialog::getOpenFileName(this,
			tr("Load Session"),
			m_last_directory,
			QStringLiteral("Session Files (%1)").arg(SESSION_FILE_PATTERN));
		m_tagger.openSession(filename);
	});
}

void Window::createMenus()
{
	auto add_action = [this](auto& object,auto& action)
	{
		this->addAction(&action);
		(&object)->addAction(&action);
	};
	auto add_separator = [](auto& object)
	{
		(&object)->addSeparator();
	};

	add_action(menu_file, a_open_file);
	add_action(menu_file, a_open_dir);
	add_action(menu_file, a_load_session);
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

	add_action(menu_navigation, a_next_file);
	add_action(menu_navigation, a_prev_file);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_save_next);
	add_action(menu_navigation, a_save_prev);
	add_separator(menu_navigation);
	add_action(menu_navigation, a_open_loc);
	add_action(menu_navigation, a_reload_tags);

	add_action(menu_view, a_view_fullscreen);
	add_separator(menu_view);
	add_action(menu_view, a_view_menu);
	add_action(menu_view, a_view_input);
	add_action(menu_view, a_view_statusbar);

	add_action(menu_options, a_ib_replace);
	add_action(menu_options, a_ib_restore);
	add_separator(menu_options);
	menu_options.addMenu(&menu_options_language);
	menu_options.addMenu(&menu_options_style);
	add_separator(menu_options);

	auto lang_group = new QActionGroup(&menu_options_language);
	lang_group->setExclusive(true);
	auto style_group = new QActionGroup(&menu_options_style);
	style_group->setExclusive(true);

	QSettings settings;


	QDirIterator it_locale(QStringLiteral(":/i18n/"));
	while(it_locale.hasNext()) {
		if(!it_locale.next().endsWith(QStringLiteral(".qm"))) {
			continue;
		}
		auto name = it_locale.fileInfo().completeBaseName();
		auto code = name.right(2);
		name.truncate(name.size() - 3);

		auto action = new QAction(name, this);
		action->setCheckable(true);
		action->setData(code);
		menu_options_language.addAction(action);
		lang_group->addAction(action);

		if(code == settings.value(SETT_LOCALE, QStringLiteral("en")).toString())
		{
			action->setChecked(true);
		}
	}

	QDirIterator it_style(QStringLiteral(":/css"));
	while (it_style.hasNext()) {
		it_style.next();

		auto title = it_style.fileInfo().baseName();
		Q_ASSERT(!title.isEmpty());
		title[0] = title[0].toUpper();

		auto action = new QAction(title, this);
		action->setCheckable(true);
		if(settings.value(SETT_STYLE).toString() == it_style.filePath()) {
			action->setChecked(true);
		}
		action->setData(it_style.filePath());
		menu_options_style.addAction(action);
		style_group->addAction(action);
	}

	connect(lang_group, &QActionGroup::triggered, [](const QAction* a) {
		Q_ASSERT(a != nullptr);
		QSettings settings;
		settings.setValue(SETT_LOCALE, a->data().toString());
		QMessageBox::information(nullptr,
			tr("Language changed"),
			tr("<p>Please restart the application to apply language change.</p>"));
	});

	connect(style_group, &QActionGroup::triggered, [this](const QAction* a)
	{
		QSettings s; s.setValue(SETT_STYLE, a->data().toString());
		loadWindowStyles();
	});

	add_action(menu_help, a_help);
	add_action(menu_help, a_stats);
	add_separator(menu_help);
	add_action(menu_help, a_about);
	add_action(menu_help, a_about_qt);

	menuBar()->addMenu(&menu_file);
	menuBar()->addMenu(&menu_navigation);
	menuBar()->addMenu(&menu_view);
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
	a_save_session.setDisabled(val);
}

//------------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(nullptr,
	tr("About %1").arg(QStringLiteral(TARGET_PRODUCT)),
	tr(util::read_resource_html("about.html")).arg(
		QStringLiteral(TARGET_PRODUCT),
		qApp->applicationVersion(),
		QStringLiteral(__DATE__),
		QStringLiteral(__TIME__)));
}

void Window::help()
{
	QSettings settings;
	bool portable = settings.value(QStringLiteral("settings-portable"), false).toBool();

	QMessageBox::information(nullptr,
	tr("Help"),
	tr(util::read_resource_html("help.html")).arg(
		a_ib_replace.text().remove('&'),
		a_ib_restore.text().remove('&'),
		m_reverse_search.proxyEnabled() ? tr("enabled", "proxy") : tr("disabled", "proxy"),
		m_reverse_search.proxyEnabled() ? tr(", proxy URL: <code>") + m_reverse_search.proxyURL() : tr("<code>"),
		portable ? tr("enabled", "portable") : tr("disabled", "portable"),
		QStringLiteral(TARGET_PRODUCT)));
}

//------------------------------------------------------------------------
