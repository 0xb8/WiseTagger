/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QDirIterator>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QMimeData>

#include <QKeySequence>
#include <QMessageBox>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include "window.h"

const static char* MainWindowTitle = "%1  –  WiseTagger v%2";
const static int FilesVectorReservedSize = 1024;

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
	, savePrevAct(	tr("Save and open previous file"),this)
	, reloadTagsAct(tr("Reload tag file"),this)
	, iqdbSearchAct(tr("IQDB.org search"), this)
	, exitAct(	tr("Exit"), this)
	, aboutAct(	tr("About"),this)
	, aboutQtAct(	tr("About Qt"),this)
	, helpAct(	tr("Help"),this)
	, fileMenu(	tr("File"), this)
	, helpMenu(	tr("Help"), this)
{
	tagger.installEventFilter(this);
	tagger.installEventFilterForPicture(this);

	setCentralWidget(&tagger);
	setAcceptDrops(true);

	createActions();
	createMenus();
	files.reserve(FilesVectorReservedSize);
	parseCommandLineArguments();

	resize(1024,600);
}

Window::~Window() { }

//------------------------------------------------------------------------
void Window::openSingleFile(const QString &file) {
	if(tagger.loadFile(file)) {
		enableMenusOnFileOpen();
		setWindowTitle(tr(MainWindowTitle).arg(tagger.currentFileName()).arg(qApp->applicationVersion()));
	}
}

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

void Window::loadDirContents(const QString &directory)
{
	last_directory = directory;
	QDirIterator it(directory);
	QFileInfo file;
	while(it.hasNext()) {
		it.next();
		file.setFile(it.filePath());

		if(check_ext(file.suffix())) {
			files.push_back(it.filePath());
		}
	}
}

//------------------------------------------------------------------------
void Window::fileOpenDialog()
{
	QString fileName = QFileDialog::getOpenFileName(this,tr("Open File"), last_directory, tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png)"));
	if (fileName.isEmpty()) return;

	openFileFromDirectory(fileName);
}

void Window::directoryOpenDialog()
{
	QString dir = QFileDialog::getExistingDirectory(this,tr("Open Directory"),last_directory, QFileDialog::ShowDirsOnly);
	if(dir.isEmpty()) return;

	openSingleDirectory(dir);
}

//------------------------------------------------------------------------
int Window::saveFile(bool forcesave) {
	int ret = tagger.rename(forcesave);
	if(ret > 0) {
		files[current_pos] = tagger.currentFile();
		setWindowTitle(tr(MainWindowTitle).arg(tagger.currentFileName()).arg(qApp->applicationVersion()));
	}
	return ret;
}

void Window::nextFile(bool forcesave) {
	if(tagger.isModified()) {
		if(saveFile(forcesave) < 0)
			return;
	}
	++current_pos;
	if(current_pos < 0 || current_pos >= files.size()) {
		current_pos = 0;
	}

	openSingleFile(files[current_pos]);
	qDebug("%d -> %s",current_pos, qPrintable(tagger.currentFileName()));

}

void Window::prevFile(bool forcesave) {
	if(tagger.isModified()) {
		if(saveFile(forcesave) < 0)
			return;
	}

	--current_pos;
	if(current_pos < 0 || current_pos >= files.size()) {
		current_pos = files.size()-1;
	}

	openSingleFile(files[current_pos]);
	qDebug("%d <- %s",current_pos, qPrintable(tagger.currentFileName()));

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

			if( !(dropfile.isDir() || check_ext(dropfile.suffix()) ) ) {
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
		if(saveFile() >= 0) {
			event->accept();
			return;
		}
		event->ignore();
		return;
	}
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

		if(f.isFile() && check_ext(f.suffix())) {
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

//------------------------------------------------------------------------
void Window::createActions()
{
	openAct.setShortcut(tr("Ctrl+O"));
	connect(&openAct, SIGNAL(triggered()), this, SLOT(fileOpenDialog()));

	openDirAct.setShortcut(tr("Ctrl+D"));
	connect(&openDirAct, SIGNAL(triggered()), this, SLOT(directoryOpenDialog()));

	NextAct.setShortcut(QKeySequence(Qt::Key_Right));
	NextAct.setEnabled(false);
	connect(&NextAct, SIGNAL(triggered()), this, SLOT(next()));

	PrevAct.setShortcut(QKeySequence(Qt::Key_Left));
	PrevAct.setEnabled(false);
	connect(&PrevAct, SIGNAL(triggered()), this, SLOT(prev()));

	saveAct.setShortcut(tr("Ctrl+S"));
	saveAct.setEnabled(false);
	connect(&saveAct, SIGNAL(triggered()), this, SLOT(save()));

	saveNextAct.setShortcut(QKeySequence(Qt::ALT + Qt::Key_Right));
	saveNextAct.setEnabled(false);
	connect(&saveNextAct, SIGNAL(triggered()), this, SLOT(savenext()));

	savePrevAct.setShortcut(QKeySequence(Qt::ALT + Qt::Key_Left));
	savePrevAct.setEnabled(false);
	connect(&savePrevAct, SIGNAL(triggered()), this, SLOT(saveprev()));

	reloadTagsAct.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	reloadTagsAct.setEnabled(false);
	connect(&reloadTagsAct, SIGNAL(triggered()), this, SLOT(reload_tags()));

	iqdbSearchAct.setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F));
	iqdbSearchAct.setEnabled(false);
	connect(&iqdbSearchAct, SIGNAL(triggered()), this, SLOT(search_iqdb()));

	connect(&exitAct, SIGNAL(triggered()), this, SLOT(close()));
	connect(&aboutAct, SIGNAL(triggered()), this, SLOT(about()));
	connect(&aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
	connect(&helpAct, SIGNAL(triggered()), this, SLOT(help()));

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
		tr("<h3>WiseTagger v%1</h3><p>Built %2, %3.</p>\
<p>Copyright &copy; 2014 catgirl &lt;<a href=\"mailto:cat@wolfgirl.org\">cat@wolfgirl.org</a>&gt; (bugreports are very welcome!)</p>\
<p>This program is free software. It comes without any warranty, to the extent permitted by applicable law. You can redistribute it \
and/or modify it under the terms of the Do What The Fuck You Want To Public License, Version 2, as published by Sam Hocevar. \
See <a href=\"http://www.wtfpl.net/\">http://www.wtfpl.net/</a> for more details.</p>").arg(qApp->applicationVersion()).arg(__DATE__).arg(__TIME__));
}

void Window::help()
{
	QMessageBox::about(this,tr("Help"),tr("<h2>User Interface</h2>\
<p><b>Tab</b> &ndash; list autocomplete suggestions</p>\
<p><b>Enter</b> &ndash; apply changes and clear focus</p>\
<p><b>Left</b> and <b>Right</b> arrows &ndash; show previous/next picture</p>\
<p>Tag file format documentation is at <a href=\"https://bitbucket.org/catgirl/wisetagger\">project repository page</a>.</p>"));
}

void Window::save()
{
	saveFile();
}

void Window::next()
{
	nextFile();
}

void Window::prev()
{
	prevFile();
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
bool check_ext(const QString& ext) {
	static const char* const exts[] = {"jpg", "jpeg", "png", "gif", "bmp"};
	static const char len = sizeof(exts) / sizeof(*exts);
	for(int i = 0; i < len; ++i)
		if(ext == exts[i])
			return true;
	return false;
}
