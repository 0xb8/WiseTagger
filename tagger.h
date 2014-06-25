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
#include <QFileInfo>

#include "unordered_map_qt.h"
#include "picture.h"
#include "input.h"


class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *parent = nullptr);
	virtual ~Tagger();

	bool isModified() const;
	bool loadFile(const QString& filename);
	bool loadJsonConfig();

	void locateTagsFile(const QFileInfo &file);
	void reloadTags();

	int rename(bool forcesave); // -1 when save is cancelled

	void installEventFilterForPicture(QObject *filter_object);


	QString currentFile() const;
	QString currentFileName() const;
	int picture_width() const;
	int picture_height() const;
	float picture_size() const;

private:
	Picture pic;
	TagInput input;
	QFrame hr_line;
	QVBoxLayout mainlayout;
	QVBoxLayout inputlayout;

	QString current_file, current_dir, current_tags_file;
	std::unordered_map<QString,QString> dir_tagfiles;

};

#endif // WISETAGGER_H
