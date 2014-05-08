#include <QDirIterator>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QUrl>

#include <QKeySequence>
#include <QMessageBox>

#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include "window.h"

const static char* MainWindowTitle = "%1  –  WiseTagger v%2";

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
	, exitAct(	tr("Exit"), this)
	, aboutAct(	tr("About"),this)
	, fileMenu(	tr("File"), this)
	, helpMenu(	tr("Help"), this)
{
	Q_UNUSED(parent);

	tagger.installEventFilter(this);
	tagger.installEventFilterForPicture(this);

	setCentralWidget(&tagger);
	setAcceptDrops(true);

	createActions();
	createMenus();

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

void Window::openFileFromDirectory(const QString &filename)
{
	last_directory = QFileInfo(filename).absolutePath();
	files.clear();
	loadDirContents(last_directory);

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
	QString filter(tr("Image files (*.gif *.jpg *.jpeg *.jpg *.png)"));
	QString fileName = QFileDialog::getOpenFileName(this,tr("Open File"), last_directory,filter);
	if (fileName.isEmpty()) return;

	openFileFromDirectory(fileName);
}

void Window::directoryOpenDialog()
{
	QString dir = QFileDialog::getExistingDirectory(this,tr("Open Directory"),last_directory);
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
	savePrevAct.setEnabled(false);
	connect(&reloadTagsAct, SIGNAL(triggered()), this, SLOT(reload_tags()));

	exitAct.setShortcut(tr("Ctrl+Q"));
	connect(&exitAct, SIGNAL(triggered()), this, SLOT(close()));

	connect(&aboutAct, SIGNAL(triggered()), this, SLOT(about()));

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
	fileMenu.addSeparator();
	fileMenu.addAction(&exitAct);
	helpMenu.addAction(&aboutAct);

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
}

//------------------------------------------------------------------------
void Window::about() {
	QMessageBox::about(this,
			   tr("About WiseTagger"),
			   tr("<b>WiseTagger</b> v%1 by cat, built %2 %3.\
<br>Send bugreports and feature requests to \
<a href=\"mailto:cat@wolfgirl.org\">cat@wolfgirl.org</a>.\
<br><br>Tab - autocomplete options<br>\
Enter - apply filename changes and clear focus").arg(qApp->applicationVersion()).arg(__DATE__).arg(__TIME__));
}

void Window::save()
{
	saveFile(false);
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

//------------------------------------------------------------------------
bool check_ext(const QString& ext) {
	static const char* const exts[] = {"jpg", "jpeg", "png", "gif", "bmp"};
	static const char len = sizeof(exts) / sizeof(*exts);
	for(int i = 0; i < len; ++i)
		if(ext == exts[i])
			return true;
	return false;
}
