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
#include <QSizeGrip>

#include "window.h"
#include "util/open_graphical_shell.h"
#include "util/debug.h"
#include "util/size.h"

Window::Window(QWidget *_parent) :
	QMainWindow(_parent)
	, m_tagger(this)
	, m_reverse_search(this)
	, a_open_file(	tr("Open File..."), nullptr)
	, a_open_dir(	tr("Open Directory..."), nullptr)
	, a_delete_file(tr("Delete current file"), nullptr)
	, a_open_post(	tr("Open imageboard post"), nullptr)
	, a_iqdb_search(tr("IQDB.org search"), nullptr)
	, a_exit(	tr("Exit"), nullptr)
	, a_next_file(	tr("Next file"), nullptr)
	, a_prev_file(	tr("Previous file"), nullptr)
	, a_save_file(	tr("Save"), nullptr)
	, a_save_next(	tr("Save and open next file"), nullptr)
	, a_save_prev(	tr("Save and open previous file"), nullptr)
	, a_open_loc(	tr("Open file location"), nullptr)
	, a_reload_tags(tr("Reload tag file"), nullptr)
	, a_ib_replace(	tr("Replace imageboard tags"), nullptr)
	, a_ib_restore(	tr("Restore imageboard tags"), nullptr)
	, a_toggle_statusbar(tr("Toggle status bar"), nullptr)
	, a_about(	tr("About"), nullptr)
	, a_about_qt(	tr("About Qt"), nullptr)
	, a_help(	tr("Help"), nullptr)
	, menu_file(	tr("File"))
	, menu_navigation(tr("Navigation"))
	, menu_options(	tr("Options"))
	, menu_help(	tr("Help"))
	, m_statusbar(nullptr)
	, m_statusbar_info_label(nullptr)
{
	setCentralWidget(&m_tagger);
	setAcceptDrops(true);

	createActions();
	createMenus();

	loadWindowSettings();
	parseCommandLineArguments();
}

Window::~Window() { }

//------------------------------------------------------------------------------
/* just load picture into tagger */
void Window::openSingleFile(const QString &filename)
{
	if(m_tagger.loadFile(filename)) {
		m_post_url = m_tagger.postURL();
		enableMenusOnFileOpen();
		updateWindowTitle();
	} else {
		// FIXME: may crash when no files left in folder
		pdbg << "erasing re{named,moved} files from queue";

		auto rem = std::next(m_files.begin(), m_current_file_index);
		if(rem >= m_files.end())
			return;
		m_files.erase(std::remove(rem, m_files.end(), filename));
	}
}
/* open first file in directory */
void Window::openSingleDirectory(const QString &directory)
{
	QDirIterator it(directory);
	QFileInfo file;
	while(it.hasNext()) {
		it.next();
		file.setFile(it.filePath());
		if(check_ext(file.suffix())) {
			openFileFromDirectory(it.filePath());
			break;
		}
	}
}

/* open file and enqueue rest of directory */
void Window::openFileFromDirectory(const QString &filename)
{
	m_files.clear();
	loadDirContents(QFileInfo(filename).absolutePath());

	const auto pos = std::find(m_files.cbegin(), m_files.cend(), filename);
	Q_ASSERT(pos <= m_files.cend());

	m_current_file_index = std::distance(m_files.cbegin(), pos);
	openSingleFile(*pos);
}

/* enqueue files in directory */
void Window::loadDirContents(const QString &directory)
{
	m_last_directory = directory;
	QDirIterator it(directory);

	QFileInfo file;
	while(it.hasNext() && !it.next().isNull()) {
		file.setFile(it.filePath());

		if(Window::check_ext(file.suffix())) {
			m_files.push_back(it.filePath());
		}
	}

	QCollator collator;
	collator.setNumericMode(true);

	std::sort(m_files.begin(), m_files.end(),
		[&collator](const auto& a, const auto& b)
		{
			return collator.compare(a,b) < 0;
		});
}

//------------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	QString fileName = QFileDialog::getOpenFileName(this,
		tr("Open File"),
		m_last_directory,
		tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png)"));

	if(fileName.isEmpty())
		return;

	openFileFromDirectory(fileName);
}

void Window::directoryOpenDialog()
{
	QString dir = QFileDialog::getExistingDirectory(nullptr,
		tr("Open Directory"),
		m_last_directory,
		QFileDialog::ShowDirsOnly);

	if(dir.isEmpty())
		return;

	openSingleDirectory(dir);
}

//------------------------------------------------------------------------------
Tagger::RenameStatus Window::saveFile(bool autosave, bool show_cancel_button)
{
	auto ret = m_tagger.rename(autosave, show_cancel_button);
	if(ret == Tagger::RenameStatus::Renamed) {
		updateWindowTitle();
		m_files[m_current_file_index] = m_tagger.currentFile();
	}
	return ret;
}

void Window::nextFile(bool autosave)
{
	if(m_tagger.isModified()) {
		if(saveFile(autosave) == Tagger::RenameStatus::Cancelled)
			return;
	}
	++m_current_file_index;
	if(m_current_file_index >= m_files.size()) {
		m_current_file_index = 0;
	}

	if(!m_files.empty())
		openSingleFile(m_files[m_current_file_index]);
}

void Window::prevFile(bool autosave) {
	if(m_tagger.isModified()) {
		if(saveFile(autosave) == Tagger::RenameStatus::Cancelled)
			return;
	}

	--m_current_file_index;
	if(m_current_file_index >= m_files.size()) {
		m_current_file_index = m_files.size()-1;
	}

	if(!m_files.empty())
		openSingleFile(m_files[m_current_file_index]);
}

void Window::deleteCurrentFile()
{
	QMessageBox delete_msgbox(QMessageBox::Question, tr("Delete file?"),
		tr("<p>Are you sure you want to delete <b>%1</b> permanently?</p>"
		   "<dd><dl>File type: %2</dl>"
		   "<dl>File size: %3</dl>"
		   "<dl>Dimensions: %4 x %5</dl>"
		   "<dl>Modified: %6</dl></dd>")
			.arg(m_tagger.currentFileName())
			.arg(m_tagger.currentFileType())
			.arg(util::size::printable(m_tagger.picture_size()))
			.arg(m_tagger.picture_width())
			.arg(m_tagger.picture_height())
			.arg(QFileInfo(m_tagger.currentFile()).lastModified().toString(tr("yyyy-MM-dd hh:mm:ss"))),
		QMessageBox::Save|QMessageBox::Discard);
	delete_msgbox.setButtonText(QMessageBox::Save, tr("Delete"));

	auto reply = delete_msgbox.exec();

	if(reply == QMessageBox::Save) {
		QFile to_delete(m_files[m_current_file_index]);
		to_delete.remove();

		auto pos = std::next(m_files.begin(), m_current_file_index);
		m_files.erase(pos);
		--m_current_file_index;
		nextFile(false);
	}
}

void Window::updateWindowTitle()
{
	setWindowTitle(tr(Window::MainWindowTitle)
		.arg(m_tagger.currentFileName())
		.arg(qApp->applicationVersion())
		.arg(m_tagger.picture_width())
		.arg(m_tagger.picture_height())
		.arg(util::size::printable(m_tagger.picture_size())));

	updateStatusBarText();
}

void Window::updateWindowTitleProgress(int progress)
{
	setWindowTitle(tr(Window::MainWindowTitleProgress)
		.arg(m_tagger.currentFileName())
		.arg(qApp->applicationVersion())
		.arg(m_tagger.picture_width())
		.arg(m_tagger.picture_height())
		.arg(util::size::printable(m_tagger.picture_size()))
		       .arg(progress));
}

void Window::updateStatusBarText()
{
	QString statusbar_text;

	statusbar_text.append(tr("Reverse search proxy is "));
	statusbar_text.append(m_reverse_search.proxyEnabled()
			? tr("<b>enabled</b>, proxy URL: <code>%1</code>. ").arg(m_reverse_search.proxyURL())
			: tr("<b>disabled</b>. "));
	m_statusbar_info_label.setText(statusbar_text);
}

void Window::updateImageboardPostURL(QString url)
{
	if(!url.isEmpty()) {
		m_post_url = url;
		a_open_post.setEnabled(true);
	} else {
		m_post_url.clear();
		a_open_post.setEnabled(false);
	}
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
			if(!(dropfile.isDir() || Window::check_ext(dropfile.suffix()))) {
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
				openFileFromDirectory(dropfile.absoluteFilePath());

			if(dropfile.isDir())
				openSingleDirectory(dropfile.absoluteFilePath());

			return true;
		}

		m_files.clear();
		for(auto&& fileurl : fileurls) {
			dropfile.setFile(fileurl.toLocalFile());

			if(dropfile.isFile() && Window::check_ext(dropfile.suffix()))
				m_files.push_back(dropfile.absoluteFilePath());

			if(dropfile.isDir())
				loadDirContents(dropfile.absoluteFilePath());
		}

		pdbg << "loaded" << m_files.size() << "images from" << fileurls.size() << "dropped items";

		openSingleFile(m_files.front());
		m_current_file_index = 0;
		return true;
	}
	return false;
}

void Window::closeEvent(QCloseEvent *e)
{
	if(m_tagger.isModified()) {
		auto r = saveFile(false);
		if(r == Tagger::RenameStatus::Renamed || r == Tagger::RenameStatus::Failed) {
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

void Window::parseCommandLineArguments()
{
	QFileInfo f;
	QStringList args = qApp->arguments();

	QCommandLineParser parser;
	parser.addVersionOption();
	parser.addHelpOption();
	QCommandLineOption proxy_option("proxy", "Set proxy for IQDB search", "proxy");
	QCommandLineOption no_proxy_option("no-proxy", "Disable proxy");
	parser.addOption(proxy_option);
	parser.addOption(no_proxy_option);
	parser.process(args);

	if(parser.isSet(proxy_option)) {
		QString proxy_arg = parser.value("proxy");
		pdbg << "proxy arg" << proxy_arg;
		m_reverse_search.setProxy({proxy_arg, QUrl::StrictMode});
	}

	if(parser.isSet(no_proxy_option)) {
		pdbg << "--no-proxy";
		m_reverse_search.setProxyEnabled(false);
	}

	for(const auto& arg : parser.positionalArguments()) {
		if(arg.startsWith('-')) {
			continue;
		}
		f.setFile(arg);

		if(!f.exists()) {
			continue;
		}

		if(f.isFile() && Window::check_ext(f.suffix())) {
			m_files.push_back(f.absoluteFilePath());
		}

		if(f.isDir()) {
			loadDirContents(f.absoluteFilePath());
		}
	}

	if(m_files.size() == 1) {
		openFileFromDirectory(m_files.front());
		return;
	}

	if(!m_files.empty()) {
		openSingleFile(m_files.front());
		m_current_file_index = 0;
	}
}

void Window::saveWindowSettings()
{
	QSettings settings;
	settings.beginGroup("window");
		settings.setValue("size", this->size());
		settings.setValue("position", this->pos());
		settings.setValue("maximized", this->isMaximized());
		settings.setValue("last-directory", m_last_directory);
	settings.endGroup();
}

void Window::loadWindowSettings()
{
	// check if running in portable mode
	if(QFile(qApp->applicationDirPath() + "/portable.dat").exists()) {
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			qApp->applicationDirPath() + "/settings");
	}

	QSettings settings;
	settings.beginGroup("window");
		m_last_directory = settings.value("last-directory").toString();
		resize(settings.value("size", QSize(1024,600)).toSize());
		if(settings.contains("position"))
			move(settings.value("position").toPoint());
		if(settings.value("maximized", false).toBool())
			showMaximized();
	settings.endGroup();

	a_ib_replace.setChecked(
		settings.value("imageboard/replace-tags", false).toBool());
	a_ib_restore.setChecked(
		settings.value("imageboard/restore-tags", true).toBool());
	a_toggle_statusbar.setChecked(
		settings.value("window/show-statusbar", false).toBool());

	m_statusbar.setVisible(settings.value("window/show-statusbar", false).toBool());
}

//------------------------------------------------------------------------------
void Window::createActions()
{
	a_open_file.setShortcut(    Qt::CTRL + Qt::Key_O);
	a_open_dir.setShortcut(     Qt::CTRL + Qt::Key_D);
	a_next_file.setShortcut(    QKeySequence(Qt::Key_Right));
	a_prev_file.setShortcut(    QKeySequence(Qt::Key_Left));
	a_save_file.setShortcut(    Qt::CTRL + Qt::Key_S);
	a_save_next.setShortcut(    QKeySequence(Qt::ALT + Qt::Key_Right));
	a_save_prev.setShortcut(    QKeySequence(Qt::ALT + Qt::Key_Left));
	a_delete_file.setShortcut(  QKeySequence(Qt::Key_Delete));
	a_reload_tags.setShortcut(  QKeySequence(Qt::CTRL + Qt::Key_R));
	a_open_post.setShortcut(    QKeySequence(Qt::CTRL + Qt::Key_P));
	a_iqdb_search.setShortcut(  QKeySequence(Qt::CTRL + Qt::Key_F));
	a_open_loc.setShortcut(	    QKeySequence(Qt::CTRL + Qt::Key_L));
	a_help.setShortcut(         Qt::Key_F1);

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

	connect(&a_open_file,   &QAction::triggered, this, &Window::fileOpenDialog);
	connect(&a_open_dir,    &QAction::triggered, this, &Window::directoryOpenDialog);
	connect(&a_delete_file, &QAction::triggered, this, &Window::deleteCurrentFile);
	connect(&a_exit,        &QAction::triggered, this, &QWidget::close);
	connect(&a_about,       &QAction::triggered, this, &Window::about);
	connect(&a_about_qt,    &QAction::triggered, qApp, &QApplication::aboutQt);
	connect(&a_help,        &QAction::triggered, this, &Window::help);
	connect(&a_reload_tags, &QAction::triggered, &m_tagger, &Tagger::reloadTags);

	connect(&m_reverse_search, &ReverseSearch::uploadProgress, this, &Window::showUploadProgress);
	connect(&m_reverse_search, &ReverseSearch::finished,       this, &Window::hideUploadProgress);
	connect(&m_tagger,         &Tagger::postURLChanged,        this, &Window::updateImageboardPostURL);

	connect(&a_save_file,   &QAction::triggered, [this]()
	{
		this->saveFile(false, false); // !autosave, !show_cancel_button
	});
	connect(&a_next_file,   &QAction::triggered, [this]()
	{
		nextFile(false); // !autosave
	});
	connect(&a_prev_file,   &QAction::triggered, [this]()
	{
		prevFile(false); // !autosave
	});
	connect(&a_save_next,   &QAction::triggered, [this]()
	{
		nextFile(true); // autosave
	});
	connect(&a_save_prev,   &QAction::triggered, [this]()
	{
		prevFile(true); //autosave
	});
	connect(&a_open_post,   &QAction::triggered, [this]()
	{
		QDesktopServices::openUrl(this->m_post_url);
	});
	connect(&a_iqdb_search, &QAction::triggered, [this]()
	{
		this->m_reverse_search.search(m_tagger.currentFile());
	});
	connect(&a_open_loc,    &QAction::triggered, [this]()
	{
		util::open_file_in_gui_shell(this->m_tagger.currentFile());
	});
	connect(&a_ib_replace,  &QAction::triggered, [](bool checked)
	{
		QSettings settings;
		settings.setValue("imageboard/replace-tags", checked);
	});
	connect(&a_ib_restore,  &QAction::triggered, [](bool checked)
	{
		QSettings settings;
		settings.setValue("imageboard/restore-tags", checked);
	});
	connect(&a_toggle_statusbar, &QAction::triggered, [this](bool checked)
	{
		QSettings settings;
		settings.setValue("window/show-statusbar", checked);
		this->m_statusbar.setVisible(checked);
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
	m_statusbar.setStyleSheet("border-top: 1px outset grey; background: rgba(0,0,0,0.1);");
	m_statusbar_info_label.setStyleSheet("background: transparent; border: none;");
	m_statusbar.setSizeGripEnabled(false);
	m_statusbar.addPermanentWidget(&m_statusbar_info_label);
}

void Window::enableMenusOnFileOpen()
{
	a_next_file.setEnabled(true);
	a_prev_file.setEnabled(true);
	a_save_file.setEnabled(true);
	a_save_next.setEnabled(true);
	a_save_prev.setEnabled(true);
	a_delete_file.setEnabled(true);
	a_reload_tags.setEnabled(true);
	a_open_post.setDisabled(m_post_url.isEmpty());
	a_iqdb_search.setEnabled(true);
	a_open_loc.setEnabled(true);
}

//------------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(nullptr,
	tr("About WiseTagger"),
	tr(	"<h3>WiseTagger v%1</h3><p>Built %2, %3.</p><p>Copyright &copy; 2015 catgirl &lt;"
		"<a href=\"mailto:cat@wolfgirl.org\">cat@wolfgirl.org</a>&gt; (bugreports are very welcome!)</p>"
		"<p>This program is free software. It comes without any warranty, to the extent permitted by applicable law. "
		"You can redistribute it and/or modify it under the terms of the Do What The Fuck You Want To Public License, "
		"Version 2, as published by Sam Hocevar. See <a href=\"http://www.wtfpl.net\">http://www.wtfpl.net/</a> "
		"for more details.</p>"
	).arg(qApp->applicationVersion()).arg(__DATE__).arg(__TIME__));
}

void Window::help()
{
	QMessageBox::about(nullptr,
	tr("Help"),
	tr(	"<h2>User Interface</h2>"
		"<p><b>Tab</b> &ndash; list autocomplete suggestions</p>"
		"<p><b>Enter</b> &ndash; apply changes and clear focus</p>"
		"<p><b>Left</b> and <b>Right</b> arrows &ndash; show previous/next picture</p>"
		"<p>More documentation at <a href=\"https://bitbucket.org/catgirl/wisetagger\">project repository page</a>.</p>"
	));
}

//------------------------------------------------------------------------
bool Window::check_ext(const QString& ext)
{
	static const char* const exts[] = {"jpg", "jpeg", "png", "gif", "bmp"};
	static const char len = util::size::array_size(exts);
	for(int i = 0; i < len; ++i)
		if(QString::compare(ext, exts[i], Qt::CaseInsensitive) == 0)
			return true;
	return false;
}
