#ifndef WINDOW_H
#define WINDOW_H

#include <QMainWindow>
#include "tagger.h"
#include <QMenuBar>
#include <QAction>
#include <QString>
#include <QVector>
#include <QMenu>

class Window : public QMainWindow {
	Q_OBJECT
public:
	explicit Window(QWidget *parent = 0);
	virtual ~Window();

	void openFileFromDirectory(const QString &filename);	// load dir contents and open selected file
	void openSingleFile(const QString &file);		// load file into tagger w/o adding it to queue
	void openSingleDirectory(const QString &directory);	// open first file in directory
	void loadDirContents(const QString &directory);		// enqueue files in directory
	int  saveFile(bool forcesave);
	void nextFile(bool forcesave);
	void prevFile(bool forcesasve);

protected:
	bool eventFilter(QObject *, QEvent *);

private slots:
	void fileOpenDialog();
	void directoryOpenDialog();
	void about();
	void save();
	void next();
	void prev();
	void savenext();
	void saveprev();
	void reload_tags();

private:
	void createActions();
	void createMenus();
	void enableMenusOnFileOpen();

	Tagger tagger;
	QString last_directory;
	QVector<QString> files;
	int current_pos;

	QAction openAct;
	QAction openDirAct;
	QAction NextAct;
	QAction PrevAct;
	QAction saveAct;
	QAction saveNextAct;
	QAction savePrevAct;
	QAction reloadTagsAct;
	QAction exitAct;
	QAction aboutAct;

	QMenu fileMenu;
	QMenu helpMenu;
};

//-------------------------------------------------------------------
bool check_ext(const QString& ext);

#endif // WINDOW_H
