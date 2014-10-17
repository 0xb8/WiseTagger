/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include "reverse_search.h"
#include "tagger.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QString>
#include <QVector>
#include <QMenu>

class Window : public QMainWindow {
	Q_OBJECT
public:
	explicit Window(QWidget *parent = nullptr);
	virtual ~Window();

	static bool check_ext(const QString& ext);

	void openFileFromDirectory(const QString filename);	// load dir contents and open selected file
	void openSingleFile(const QString &file);		// load file into tagger w/o adding it to queue
	void openSingleDirectory(const QString &directory);	// open first file in directory
	void loadDirContents(const QString &directory);		// enqueue files in directory
	int  saveFile(bool autosave, bool show_cancel_button = true);
	void nextFile(bool autosave);
	void prevFile(bool autosave);

protected:
	bool eventFilter(QObject *, QEvent *);
	void closeEvent(QCloseEvent *event);

private slots:
	void fileOpenDialog();
	void directoryOpenDialog();
	void openConfigFile();
	void openImageLocation();
	void about();
	void help();
	void save();
	void next();
	void prev();
	void savenext();
	void saveprev();
	void reload_tags();
	void search_iqdb();

private:
	void createActions();
	void createMenus();
	void enableMenusOnFileOpen();
	void parseCommandLineArguments();
	void updateWindowTitle();

	void readJsonConfig();
	void writeJsonConfig();

	static constexpr char const* MainWindowTitle = "%1 [%3x%4] (%5)  –  WiseTagger v%2";
	static constexpr char const* ConfigFilename = "wisetagger.json";
	static const int FilesVectorReservedSize = 1024;

	Tagger tagger;
	ReverseSearch iqdb;

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
	QAction iqdbSearchAct;
	QAction exitAct;
	QAction aboutAct;
	QAction aboutQtAct;
	QAction helpAct;
	QAction openConfigAct;
	QAction openFileLocation;

	QMenu fileMenu;
	QMenu navMenu;
	QMenu helpMenu;
};
#endif // WINDOW_H
