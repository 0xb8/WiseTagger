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

#include "unordered_map_qt.h"
#include "picture.h"
#include "input.h"


class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *parent = nullptr);
	virtual ~Tagger();

	bool loadFile(const QString& filename);
	int  rename(bool autosave, bool show_cancel_button = true); // 1 = success, -1 = cancelled, 0 = failed

	void reloadTags();
	void clearDirTagfiles();
	void insertToDirTagfiles(const QString& dir, const QString& tagfile);

	void setFont(const QFont& f);
	const QFont &font() const;
	bool isModified() const;
	int picture_width() const;
	int picture_height() const;
	qint64 picture_size() const;
	QString currentFile() const;
	QString currentFileName() const;

private:

	void locateTagsFile(const QString& file);
	Picture m_picture;
	TagInput m_input;
	QFrame hr_line;
	QVBoxLayout mainlayout;
	QVBoxLayout inputlayout;

	QString m_current_file, m_current_dir, m_current_tags_file;
	std::unordered_map<QString,QString> tag_files_for_directories;

};

#endif // WISETAGGER_H
