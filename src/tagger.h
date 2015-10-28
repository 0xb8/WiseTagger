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

class Tagger : public QWidget
{
	Q_OBJECT

public:
	enum class RenameStatus : std::int8_t {
		Cancelled = -1,
		Failed = 0,
		Renamed = 1
	};

	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override;

	bool loadFile(const QString& filename);
	Tagger::RenameStatus rename(bool force_save, bool show_cancel_button = true);

	void reloadTags();

	bool isModified() const;
	int picture_width() const;
	int picture_height() const;
	qint64 picture_size() const;

	QString postURL() const;
	QString currentFile() const;
	QString currentText() const;
	QString currentFileName() const;


signals:
	void postURLChanged(QString post_url);

private:

	void findTagsFiles();
	QFrame hr_line;
	Picture m_picture;
	TagInput m_input;
	QVBoxLayout mainlayout;
	QVBoxLayout inputlayout;

	QString m_current_file, m_current_dir;
	QStringList m_current_tag_files;

};

#endif // WISETAGGER_H
