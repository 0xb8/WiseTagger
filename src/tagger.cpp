/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "tagger.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>

Tagger::Tagger(QWidget *_parent) :
	QWidget(_parent)
{
	installEventFilter(parent);
	m_picture.installEventFilter(parent);

	mainlayout.setMargin(0);
	mainlayout.setSpacing(0);

	inputlayout.setMargin(10);
	inputlayout.addWidget(&m_input);

	hr_line.setFrameStyle(QFrame::HLine | QFrame::Sunken);
	mainlayout.addWidget(&m_picture);
	mainlayout.addWidget(&hr_line);
	mainlayout.addLayout(&inputlayout);
	setLayout(&mainlayout);
	setAcceptDrops(true);
}

Tagger::~Tagger() { }

QString Tagger::currentFile() const
{
	return m_current_file;
}


QString Tagger::currentFileName() const
{
	return QFileInfo(m_current_file).fileName();
}


int Tagger::picture_width() const
{
	return m_picture.width();
}


int Tagger::picture_height() const
{
	return m_picture.height();
}


bool Tagger::isModified() const
{
	return m_input.text() != QFileInfo(m_current_file).completeBaseName() && !m_current_file.isEmpty();
}


qint64 Tagger::picture_size() const
{
	return QFileInfo(m_current_file).size();
}

void Tagger::reloadTags()
{
	m_input.loadTagFile(m_current_tags_file);
	m_input.reloadAdditionalTags();
}

void Tagger::clearDirTagfiles()
{
	tag_files_for_directories.clear();
}

void Tagger::insertToDirTagfiles(const QString &dir, const QString &tagfile)
{
	tag_files_for_directories.insert(std::pair<QString,QString>(dir,tagfile));
}

void Tagger::setFont(const QFont &f)
{
	m_input.setFont(f);
}

const QFont &Tagger::font() const
{
	return m_input.font();
}

void Tagger::locateTagsFile(const QString &file)
{
	QFileInfo fi(file);
	/* If loading file from other directory */
	if(m_current_dir != fi.absolutePath()) {
		/* Determine file's parent directory */
		m_current_dir = fi.absolutePath();
		bool found = false;
		QFileInfo directory;

		for(auto&& entry : tag_files_for_directories) {
			directory.setFile(entry.first);

			/* Check if file's parent dir is inside mapped dir */
			if(directory.exists() && m_current_dir.contains(directory.absoluteFilePath())) {
				m_current_tags_file = entry.second;
				found = true;
				break;
			}
		}
		if(!found) {
			/* Use global tags file */
			auto it = tag_files_for_directories.find("*");
			if(it != tag_files_for_directories.end()) {
				m_current_tags_file = it->second;
			} else {
				m_current_tags_file = "tags.txt";
			}
		}

		m_input.loadTagFile(m_current_tags_file);
		return;

	} // if(current_dir...

	m_input.reloadAdditionalTags();
	return;
}


bool Tagger::loadFile(const QString &filename)
{
	QFileInfo f(filename);
	if(!f.exists()) {
		QMessageBox::critical(this,
			tr("Error opening image"),
			tr("File <b>%1</b> does not exist.").arg(f.fileName()));
		return false;
	}

	if(!m_picture.loadPicture(f.absoluteFilePath())) {
		QMessageBox::critical(this,
			tr("Error opening image"),
			tr("File format is not supported or file is corrupted."));
			return false;
	}

	locateTagsFile(filename);
	m_input.setText(f.completeBaseName());
	m_current_file = f.absoluteFilePath();
	m_picture.setFocus();
	return true;
}


int Tagger::rename(bool autosave, bool show_cancel_button)
{
	QFileInfo file(m_current_file);
	QString new_file_path;

	m_input.fixTags();
	/* Make new file path from input text */
	new_file_path = m_input.text() + "." + file.suffix();
	new_file_path = QFileInfo(QDir(file.canonicalPath()), new_file_path).filePath();

	if(new_file_path == m_current_file || m_input.text().isEmpty())
		return 0; /* No need to save */

	/* Check for possible filename conflict */
	if(QFileInfo::exists(new_file_path)) {
		QMessageBox::critical(this,
			tr("Cannot rename file"),
			tr("<p>File with this name already exists in <b>%1</b>.</p><p>Please change some of your tags.</p>")
				.arg(file.canonicalPath()));
		return 0;
	}

	if(autosave) {
		QFile(m_current_file).rename(new_file_path);
		m_current_file = new_file_path;
		return 1; // saved
	}

	/* Show save dialog */
	QMessageBox renameMessageBox(QMessageBox::Question,
		tr("Rename file?"),
		tr("Rename <b>%1</b>?").arg(file.completeBaseName()),
		QMessageBox::Save|QMessageBox::Discard);

	renameMessageBox.setButtonText(QMessageBox::Save, tr("Rename"));
	if(show_cancel_button) {
		renameMessageBox.addButton(QMessageBox::Cancel);
	}

	int reply = renameMessageBox.exec();

	if(reply == QMessageBox::Save) {
		QFile(m_current_file).rename(new_file_path);
		m_current_file = new_file_path;
		return 1; // saved
	}

	if(reply == QMessageBox::Cancel) {
		return -1; // cancelled
	}

	return 0;
}
