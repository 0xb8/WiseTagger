/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QDesktopServices>
#include <QDirIterator>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QMimeData>

#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonValueRef>
#include <QJsonObject>

#include <QKeySequence>
#include <QMessageBox>
#include <QJsonDocument>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include "window.h"

Window::Window(QWidget *parent) :
	QMainWindow(parent)
	, tagger(this)
	, last_directory(QDir::homePath())
	, openAct(	tr("Open File..."), this)
	, openDirAct(	tr("Open Directory..."), this)
	, NextAct(	tr("Next file"), this)
	, PrevAct(	tr("Previous file"), this)
	, saveAct(	tr("Save"), this)
	, saveNextAct(	tr("Save and open next file"), this)
	, savePrevAct(	tr("Save and open previous file"), this)
	, reloadTagsAct(tr("Reload tag file"), this)
	, iqdbSearchAct(tr("IQDB.org search"), this)
	, exitAct(	tr("Exit"), this)
	, aboutAct(	tr("About"), this)
	, aboutQtAct(	tr("About Qt"), this)
	, helpAct(	tr("Help"), this)
	, openConfig(	tr("Open configuration file"), this)
	, fileMenu(	tr("File"), this)
	, helpMenu(	tr("Help"), this)
{
	setCentralWidget(&tagger);
	setAcceptDrops(true);

	createActions();
	createMenus();

	files.reserve(FilesVectorReservedSize);
	parseCommandLineArguments();
	readJsonConfig();
}

Window::~Window() { }

//------------------------------------------------------------------------------
/* load file into tagger w/o adding it to queue */
void Window::openSingleFile(const QString &file) {
	if(tagger.loadFile(file)) {
		enableMenusOnFileOpen();
		updateWindowTitle();
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

/* load dir contents and open selected file */
void Window::openFileFromDirectory(const QString filename)
{
	last_directory = QFileInfo(filename).absolutePath();
	files.clear();
	loadDirContents(last_directory);

	current_pos = 0;

	for(int i = 0; i < files.size(); ++i) {
		if(files[i] == filename) {
			current_pos = i;
			break;
		}
	}

	openSingleFile(filename);
}

/* enqueue files in directory */
void Window::loadDirContents(const QString &directory)
{
	last_directory = directory;
	QDirIterator it(directory);
	QFileInfo file;
	while(it.hasNext()) {
		it.next();
		file.setFile(it.filePath());

		if(Window::check_ext(file.suffix())) {
			files.push_back(it.filePath());
		}
	}
}

//------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	QString fileName = QFileDialog::getOpenFileName(this,
		tr("Open File"),
		last_directory,
		tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png)"));

	if (fileName.isEmpty()) return;

	openFileFromDirectory(fileName);
}

void Window::directoryOpenDialog()
{
	QString dir = QFileDialog::getExistingDirectory(this,
		tr("Open Directory"),
		last_directory,
		QFileDialog::ShowDirsOnly);

	if(dir.isEmpty()) return;

	openSingleDirectory(dir);
}

//------------------------------------------------------------------------
int Window::saveFile(bool autosave, bool show_cancel_button) {
	int ret = tagger.rename(autosave, show_cancel_button);
	if(ret > 0) {
		updateWindowTitle();
		files[current_pos] = tagger.currentFile();
	}
	return ret;
}

void Window::nextFile(bool autosave) {
	if(tagger.isModified()) {
		if(saveFile(autosave) < 0)
			return;
	}
	++current_pos;
	if(current_pos < 0 || current_pos >= files.size()) {
		current_pos = 0;
	}

	openSingleFile(files[current_pos]);
}

void Window::prevFile(bool autosave) {
	if(tagger.isModified()) {
		if(saveFile(autosave) < 0)
			return;
	}

	--current_pos;
	if(current_pos < 0 || current_pos >= files.size()) {
		current_pos = files.size()-1;
	}

	openSingleFile(files[current_pos]);
}

void Window::save()
{
	/* autosave = f, show_cancel_button = f */
	saveFile(false, false);
}

void Window::next()
{
	nextFile(false);
}

void Window::prev()
{
	prevFile(false);
}

void Window::savenext()
{
	nextFile(true);
}

void Window::saveprev()
{
	nextFile(true);
}

void Window::reload_tags()
{
	tagger.reloadTags();
}

void Window::search_iqdb()
{
	iqdb.search(tagger.currentFile());
}

//------------------------------------------------------------------------
bool Window::eventFilter(QObject *object, QEvent *event)
{
	Q_UNUSED(object);
	if(event->type() == QEvent::DragEnter) {
		QDragEnterEvent *drag_event = static_cast<QDragEnterEvent *> (event);

		if(!drag_event->mimeData()->hasUrls())
			return true;

		if(drag_event->mimeData()->urls().empty())
			return true;

		QFileInfo dropfile;

		for(auto&& dropfileurl : drag_event->mimeData()->urls()) {
			dropfile.setFile(dropfileurl.toLocalFile());

			if( !(dropfile.isDir() || Window::check_ext(dropfile.suffix()) ) ) {
				return true;
			}
		}

		drag_event->acceptProposedAction();
		return true;
	}
	if(event->type() == QEvent::Drop) {
		QDropEvent *drop_event = static_cast<QDropEvent *> (event);

		QList<QUrl> fileurls = drop_event->mimeData()->urls();
		QFileInfo fileinfo;

		// empty urls list case is checked by dragEnter
		if(fileurls.size() == 1) {
			fileinfo.setFile(fileurls.first().toLocalFile());

			if(fileinfo.isFile())
				openFileFromDirectory(fileinfo.absoluteFilePath());

			if(fileinfo.isDir())
				openSingleDirectory(fileinfo.absoluteFilePath());

			return true;
		}

		files.clear();
		for(auto&& fileurl : fileurls) {
			fileinfo.setFile(fileurl.toLocalFile());
			if(fileinfo.isFile()) {
				files.push_back(fileinfo.absoluteFilePath());
			}
			if(fileinfo.isDir()) {
				loadDirContents(fileinfo.absoluteFilePath());
			}
		}

		openSingleFile(files.front());
		current_pos = 0;
		return true;
	}
	return false;
}

void Window::closeEvent(QCloseEvent *event)
{
	if(tagger.isModified()) {
		if(saveFile(false) >= 0) {
			writeJsonConfig();
			event->accept();
			return;
		}
		event->ignore();
		return;
	}
	writeJsonConfig();
	event->accept();
}

void Window::parseCommandLineArguments()
{
	QFileInfo f;
	QStringList args = qApp->arguments();
	for(const auto& arg : args) {
		if(arg.startsWith('-')) {
			if(arg == "-h" || arg =="--help") {
				help();
			}
			continue;
		}
		f.setFile(arg);

		if(!f.exists()) {
			continue;
		}

		if(f.isFile() && Window::check_ext(f.suffix())) {
			files.push_back(f.absoluteFilePath());
		}

		if(f.isDir()) {
			loadDirContents(f.absoluteFilePath());
		}
	}

	if(files.size() == 1) {
		openFileFromDirectory(files.front());
		return;
	}

	if(!files.empty()) {
		openSingleFile(files.front());
		current_pos = 0;
	}
}

void Window::updateWindowTitle()
{
	setWindowTitle(tr(Window::MainWindowTitle)
		.arg(tagger.currentFileName())
		.arg(qApp->applicationVersion())
		.arg(tagger.picture_width())
		.arg(tagger.picture_height())
		.arg(tagger.picture_size() <= 1024*1024 // less than 1 Mb
			? QString("%1Kb").arg(tagger.picture_size() / 1024)
			: QString("%1Mb").arg(tagger.picture_size() / 1024.0f / 1024.0f, 0,'f', 3)));
}

void Window::readJsonConfig()
{
	QFile config_file(qApp->applicationDirPath() + '/' + Window::ConfigFilename);
	if(!config_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this,
			tr("Error opening configuration file"),
			tr("<p>Could not open <b>%1</b> for reading.</p><p>Tag autocomplete will not work.</p>")
				.arg(config_file.fileName()));
	}

	QSize window_size(1024,600);
	QPoint window_pos;
	bool maximized = false;

	QJsonDocument config_json;
	QJsonObject config_object,
		config_window,
		config_window_size,
		config_window_pos,
		config_proxy;
	QJsonArray config_tag_files;
	int config_version = -1;


	/* Read JSON and form root object */
	config_json = QJsonDocument::fromJson(config_file.readAll());
	config_object = config_json.object();

	/* Currently unused, read anyway... */
	if(config_object["version"].isDouble())
		config_version = config_object["version"].toInt();

	Q_UNUSED(config_version);

	/* Check if window sub-object and its sub-objects present */
	if(config_object["window"].isObject())
		config_window = config_object["window"].toObject();

	if(config_window["maximized"].isBool())
		maximized = config_window["maximized"].toBool();

	if(config_window["size"].isObject() && config_window["position"].isObject()) {
		config_window_size = config_window["size"].toObject();
		config_window_pos = config_window["position"].toObject();
	}

	if(config_window_size["width"].isDouble() && config_window_size["height"].isDouble()) {
		window_size.setWidth(config_window_size["width"].toInt());
		window_size.setHeight(config_window_size["height"].toInt());
	}

	if(config_window_pos["x"].isDouble() && config_window_pos["y"].isDouble()) {
		window_pos.setX(config_window_pos["x"].toInt());
		window_pos.setY(config_window_pos["y"].toInt());
		move(window_pos);
	}

	/* Check if proxy settings present */
	if(config_object["proxy"].isObject())
		config_proxy = config_object["proxy"].toObject();

	QString protocol, host, user, pass;
	double port;

	if(config_proxy["protocol"].isString() && config_proxy["host"].isString() && config_proxy["port"].isDouble()) {
		protocol = config_proxy["protocol"].toString();
		host = config_proxy["host"].toString();
		port = config_proxy["port"].toDouble();
		user = config_proxy["username"].toString();
		pass = config_proxy["password"].toString();
		/* Validate port & protocol*/
		if(port > 65535.0f || port < 1.0f) {
			QMessageBox::warning(this,
				tr("Configuration error"),
				tr("<p>Invalid proxy port number <b>%1</b>, should be between 1 and 65535.</p><p>Proxy will not work.</p>"));
		} else if (protocol != "http" && protocol != "socks5") {
			QMessageBox::warning(this,
				tr("Configuration error"),
				tr("<p>Invalid proxy protocol <b>%1</b>, should be either <b>http<b> or <b>socks5</b>.</p><p>Proxy will not work.</p>").arg(protocol));
		} else {
			iqdb.setProxy(protocol, host, static_cast<std::uint16_t>(port), user, pass);
		}
	}

	/* Check if last_directory present */
	if(config_object["last_directory"].isString())
		last_directory = config_object["last_directory"].toString();

	if(config_object["tag_files"].isArray())
		config_tag_files = config_object["tag_files"].toArray();

	/* Check if tag_files array is not empty */
	if(config_tag_files.isEmpty() && !config_json.isNull()) {
		QMessageBox::warning(this,
			tr("Configuration error"),
			tr("<p><b>tag_files</b> JSON array not found in configuration file.</p><p>Tag autocomplete will not work.</p>"));
	}

	/* Clear Tagger's dir-tagfile map */
	tagger.clearDirTagfiles();

	for(auto&& o : config_tag_files) {
		if(!o.isObject()) {
			QMessageBox::warning(this,
				tr("Configuration error"),
				tr("<p><b>tag_files</b> array should contain valid JSON objects.</p><p>Tag autocomplete will not work.</p>"));
			break;
		}
		auto object = o.toObject();
		if(!object["directory"].isString() || !object["tagfile"].isString()) {
			QMessageBox::warning(this,
				tr("Configuration error"),
				tr("<p><b>directory</b> and <b>tagfile</b> entries must be strings!</p><p>Tag autocomplete will not work.</p>"));
			break;
		}

		/* Insert directory and it's tag file into Tagger's map */
		tagger.insertToDirTagfiles(object["directory"].toString(), object["tagfile"].toString());
	}
	if(maximized) {
		resize(1024,600);
		showMaximized();
		return;
	}
	resize(window_size);
}

void Window::writeJsonConfig()
{
	QFile config_file(qApp->applicationDirPath() + '/' + Window::ConfigFilename);
	if(!config_file.open(QIODevice::ReadWrite | QIODevice::Text)) {
		QMessageBox::warning(this,
			tr("Could not open configuration file"),
			tr("<p>Could not open <b>%1</b> for writing.</p><p>Window size and location will not be saved.</p>")
				.arg(config_file.fileName()));
		return;
	}

	QJsonDocument config_json;
	QJsonObject config_object, config_window_size, config_window_pos, config_window;

	config_json = QJsonDocument::fromJson(config_file.readAll());
	config_object = config_json.object();


	config_window_size.insert("width", QJsonValue(width()));
	config_window_size.insert("height", QJsonValue(height()));
	config_window_pos.insert("x", QJsonValue(pos().x()));
	config_window_pos.insert("y", QJsonValue(pos().y()));

	config_window.insert("size", QJsonValue(config_window_size));
	config_window.insert("position", QJsonValue(config_window_pos));
	config_window.insert("maximized", QJsonValue(isMaximized()));
	config_object.remove("window");
	config_object.insert("window", QJsonValue(config_window));
	config_object.remove("last_directory");
	config_object.insert("last_directory", QJsonValue(last_directory));

	QByteArray config_tmp = QJsonDocument(config_object).toJson();
	config_file.resize(0);
	config_file.write(config_tmp);
}

void Window::openConfigFile()
{
	QFileInfo config_file(qApp->applicationDirPath() + '/' + Window::ConfigFilename);
	if(!config_file.exists()) {
		QMessageBox::warning(this,
			tr("Could not open configuration file"),
			tr("<p>Could not locate <b>%1</b>").arg(config_file.fileName()));
		return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(config_file.filePath()));
}

//------------------------------------------------------------------------
void Window::createActions()
{
	openAct.setShortcut(	Qt::CTRL + Qt::Key_O);
	openDirAct.setShortcut(	Qt::CTRL + Qt::Key_D);
	NextAct.setShortcut(	QKeySequence(Qt::Key_Right));
	PrevAct.setShortcut(	QKeySequence(Qt::Key_Left));
	saveAct.setShortcut(	Qt::CTRL + Qt::Key_S);
	saveNextAct.setShortcut(QKeySequence(Qt::ALT + Qt::Key_Right));
	savePrevAct.setShortcut(QKeySequence(Qt::ALT + Qt::Key_Left));
	reloadTagsAct.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	iqdbSearchAct.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F));
	helpAct.setShortcut(	Qt::Key_F1);

	NextAct.setEnabled(false);
	PrevAct.setEnabled(false);
	saveAct.setEnabled(false);
	saveNextAct.setEnabled(false);
	savePrevAct.setEnabled(false);
	reloadTagsAct.setEnabled(false);
	iqdbSearchAct.setEnabled(false);

	connect(&openAct,	SIGNAL(triggered()), this, SLOT(fileOpenDialog()));
	connect(&openDirAct,	SIGNAL(triggered()), this, SLOT(directoryOpenDialog()));
	connect(&NextAct,	SIGNAL(triggered()), this, SLOT(next()));
	connect(&PrevAct,	SIGNAL(triggered()), this, SLOT(prev()));
	connect(&saveAct,	SIGNAL(triggered()), this, SLOT(save()));
	connect(&saveNextAct,	SIGNAL(triggered()), this, SLOT(savenext()));
	connect(&savePrevAct,	SIGNAL(triggered()), this, SLOT(saveprev()));
	connect(&reloadTagsAct,	SIGNAL(triggered()), this, SLOT(reload_tags()));
	connect(&iqdbSearchAct,	SIGNAL(triggered()), this, SLOT(search_iqdb()));
	connect(&exitAct,	SIGNAL(triggered()), this, SLOT(close()));
	connect(&aboutAct,	SIGNAL(triggered()), this, SLOT(about()));
	connect(&aboutQtAct,	SIGNAL(triggered()), qApp, SLOT(aboutQt()));
	connect(&helpAct,	SIGNAL(triggered()), this, SLOT(help()));
	connect(&openConfig,	SIGNAL(triggered()), this, SLOT(openConfigFile()));
}

void Window::createMenus() {
	fileMenu.addAction(&openAct);
	fileMenu.addAction(&openDirAct);
	fileMenu.addSeparator();
	fileMenu.addAction(&NextAct);
	fileMenu.addAction(&PrevAct);
	fileMenu.addSeparator();
	fileMenu.addAction(&saveAct);
	fileMenu.addAction(&saveNextAct);
	fileMenu.addAction(&savePrevAct);
	fileMenu.addSeparator();
	fileMenu.addAction(&reloadTagsAct);
	fileMenu.addAction(&iqdbSearchAct);
	fileMenu.addSeparator();
	fileMenu.addAction(&exitAct);

	helpMenu.addAction(&helpAct);
	helpMenu.addAction(&openConfig);
	helpMenu.addSeparator();
	helpMenu.addAction(&aboutAct);
	helpMenu.addAction(&aboutQtAct);

	menuBar()->addMenu(&fileMenu);
	menuBar()->addMenu(&helpMenu);
}

void Window::enableMenusOnFileOpen() {
	NextAct.setEnabled(true);
	PrevAct.setEnabled(true);
	saveAct.setEnabled(true);
	saveNextAct.setEnabled(true);
	savePrevAct.setEnabled(true);
	reloadTagsAct.setEnabled(true);
	iqdbSearchAct.setEnabled(true);
}

//------------------------------------------------------------------------
void Window::about()
{
	QMessageBox::about(this,
		tr("About WiseTagger"),
		tr("\
<h3>WiseTagger v%1</h3><p>Built %2, %3.</p><p>Copyright &copy; 2014 catgirl \
&lt;<a href=\"mailto:cat@wolfgirl.org\">cat@wolfgirl.org</a>&gt; (bugreports are very welcome!)</p>\
<p>This program is free software. It comes without any warranty, to the extent permitted by applicable law. You can redistribute it \
and/or modify it under the terms of the Do What The Fuck You Want To Public License, Version 2, as published by Sam Hocevar. \
See <a href=\"http://www.wtfpl.net/\">http://www.wtfpl.net/</a> for more details.</p>").arg(qApp->applicationVersion()).arg(__DATE__).arg(__TIME__));
}

void Window::help()
{
	QMessageBox::about(this,
		tr("Help"),
		tr("\
<h2>User Interface</h2>\
<p><b>Tab</b> &ndash; list autocomplete suggestions</p>\
<p><b>Enter</b> &ndash; apply changes and clear focus</p>\
<p><b>Left</b> and <b>Right</b> arrows &ndash; show previous/next picture</p>\
<p>More documentation at <a href=\"https://bitbucket.org/catgirl/wisetagger\">project repository page</a>.</p>"));
}

//------------------------------------------------------------------------
bool Window::check_ext(const QString& ext) {
	static const char* const exts[] = {"jpg", "jpeg", "png", "gif", "bmp"};
	static const char len = sizeof(exts) / sizeof(*exts);
	for(int i = 0; i < len; ++i)
		if(ext == exts[i])
			return true;
	return false;
}
