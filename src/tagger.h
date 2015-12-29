/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef WISETAGGER_H
#define WISETAGGER_H

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QFrame>
#include <QWidget>

#include "util/unordered_map_qt.h"
#include "picture.h"
#include "input.h"
#include "tagger_enums.h"
#include "file_queue.h"


class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override;

	void openFile(const QString&);  /// opens specified file and enqueues rest of directory.
	void openDir (const QString&);  /// opens first file and enqueues rest of specified directory.

	RenameStatus rename(RenameFlags flags  = RenameFlags::Default);

	void nextFile(RenameFlags flags = RenameFlags::Default);
	void prevFile(RenameFlags flags = RenameFlags::Default);
	void loadCurrentFile();   /// displays currently selected file.
	void deleteCurrentFile(); /// deletes currently selected file.

	bool    fileModified()    const;
	int     pictureWidth()    const;
	int     pictureHeight()   const;
	qint64  pictureSize()     const;
	QString postURL()         const;
	QString currentFile()     const;
	QString currentDir()      const;
	QString currentText()     const;
	QString currentFileName() const;
	QString currentFileType() const;

	FileQueue& queue();

public slots:
	void reloadTags();

signals:
	void fileOpened(const QString&);
	void tagsEdited(const QString&);
	void postURLChanged(const QString&);

private:
	bool loadFile(size_t index);
	void findTagsFiles();
	void clear();

	QVBoxLayout m_main_layout;
	QVBoxLayout m_tag_input_layout;

	QFrame      m_separator;
	Picture     m_picture;
	TagInput    m_input;

	FileQueue   m_file_queue;
	QString     m_previous_dir;
	QStringList m_current_tag_files;
};

#endif // WISETAGGER_H
