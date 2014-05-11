/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

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

	void openFileFromDirectory(const QString filename);	// load dir contents and open selected file
	void openSingleFile(const QString &file);		// load file into tagger w/o adding it to queue
	void openSingleDirectory(const QString &directory);	// open first file in directory
	void loadDirContents(const QString &directory);		// enqueue files in directory
	int  saveFile(bool forcesave = false);
	void nextFile(bool forcesave = false);
	void prevFile(bool forcesave = false);

protected:
	bool eventFilter(QObject *, QEvent *);
	void closeEvent(QCloseEvent *event);

private slots:
	void fileOpenDialog();
	void directoryOpenDialog();
	void about();
	void help();
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
	void parseCommandLineArguments();

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
	QAction aboutQtAct;
	QAction helpAct;

	QMenu fileMenu;
	QMenu helpMenu;
};

//-----------------------------------------------------------------------------
bool check_ext(const QString& ext);

#endif // WINDOW_H
