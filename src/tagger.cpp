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
#include <QDesktopServices>
#include <QSettings>
#include <QDebug>
#include <algorithm>
#include "util/debug.h"
#include "window.h"

Tagger::Tagger(QWidget *_parent) :
	QWidget(_parent)
{
	installEventFilter(_parent);
	m_picture.installEventFilter(_parent);

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

	connect(&m_input, &TagInput::postURLChanged, this, &Tagger::postURLChanged);
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

QString Tagger::currentFileType() const
{
	QFileInfo fi(m_current_file);
	return fi.suffix().toUpper();
}

QString Tagger::currentText() const
{
	return m_input.text();
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

QString Tagger::postURL() const
{
	return m_input.getPostURL();
}

//------------------------------------------------------------------------------

void Tagger::reloadTags()
{
	m_input.loadTagFiles(m_current_tag_files);
	m_input.reloadAdditionalTags();
}

void Tagger::findTagsFiles()
{
	if(m_current_file.isEmpty())
		return;

	QFileInfo fi(m_current_file);
	if(m_current_dir == fi.absolutePath()) {
		m_input.reloadAdditionalTags();
		return;
	}

	QSettings settings;

	const auto tagsfile = settings.value("tags/normal", "tags.txt").toString();
	const auto override = settings.value("tags/override","tags!.txt").toString();

	m_current_dir = fi.absolutePath();

	QString parent_dir, tagpath, overridepath, errordirs = "<li>";

	int max_height = 10;
	while(max_height --> 0) {
		parent_dir = fi.absolutePath();
		errordirs += parent_dir;
		errordirs += "</li><li>";

		tagpath = parent_dir + '/' + tagsfile;
		overridepath = parent_dir + '/' + override;

		if(QFile::exists(overridepath)) {
			m_current_tag_files.push_back(overridepath);
			pdbg << "found override" << overridepath;
			break;
		}

		if(QFile::exists(tagpath)) {
			m_current_tag_files.push_back(tagpath);
			pdbg << "found tag file" << tagpath;
		}

		if(fi.absoluteDir().isRoot())
			break;

		fi.setFile(parent_dir);
	}

	m_current_tag_files.removeDuplicates();

	if(m_current_tag_files.isEmpty()) {
		QString last_resort = qApp->applicationDirPath() + "/tags.txt";
		errordirs += qApp->applicationDirPath();
		errordirs += "</li>";
		if(QFile::exists(last_resort)) {
			m_current_tag_files.push_back(last_resort);
		} else {
			QMessageBox mbox;
			mbox.setText(tr("<h3>Could not locate suitable tag file</h3>"));
			mbox.setInformativeText(tr(
				"<p>You can still browse and rename images, but tag autocomplete will not work.</p>"
				"<hr>WiseTagger will look for <em>tag files</em> in directory of the currently opened image "
				"and in directories directly above it."

				"<p>Tag files we looked for:"
				"<dd><dl>Appending tag file: <b>%1</b></dl>"
				"<dl>Overriding tag file: <b>%2</b></dl></dd></p>"
				"<p>Directories where we looked for them, in search order:"
				"<ol>%3</ol></p>"
				"<p><a href=\"https://bitbucket.org/catgirl/wisetagger/wiki/Tag%20Files\">"
				"Appending and overriding tag files documentation"
				"</a></p>")
					.arg(tagsfile)
					.arg(override)
					.arg(errordirs));
			mbox.setIcon(QMessageBox::Warning);
			mbox.exec();
		}
	}

	std::reverse(m_current_tag_files.begin(), m_current_tag_files.end());
	reloadTags();
}

bool Tagger::loadFile(const QString &filename)
{
	QFileInfo f(filename);
	if(!f.exists()) {
		QMessageBox::critical(this,
			tr("Error opening image"),
			tr("<p>File <b>%1</b> does not exist anymore.</p>"
			   "<p>It may have been renamed, moved somewhere, or removed.</p>"
			   "<p>Next file will be opened instead.</p>").arg(f.fileName()));
		m_current_file.clear();
		m_current_dir.clear();
		m_current_tag_files.clear();
		return false;
	}

	if(!m_picture.loadPicture(f.absoluteFilePath())) {
		QMessageBox::critical(this,
			tr("Error opening image"),
			tr("Error opening <b>%1</b><br/>File format is not supported or file corrupted.").arg(f.fileName()));
		m_current_file.clear();
		m_current_dir.clear();
		m_current_tag_files.clear();
		return false;
	}

	m_input.setText(f.completeBaseName());
	m_current_file = f.absoluteFilePath();
	findTagsFiles();
	m_picture.setFocus();
	return true;
}


Tagger::RenameStatus Tagger::rename(bool force_save, bool show_cancel_button)
{
	QFileInfo file(m_current_file);
	QString new_file_path;

	m_input.fixTags();
	/* Make new file path from input text */
	new_file_path = m_input.text() + "." + file.suffix();
	new_file_path = QFileInfo(QDir(file.canonicalPath()), new_file_path).filePath();

	if(new_file_path == m_current_file || m_input.text().isEmpty())
		return RenameStatus::Failed;

	/* Check for possible filename conflict */
	if(QFileInfo::exists(new_file_path)) {
		QMessageBox::critical(this,
			tr("Cannot rename file"),
			tr("<p>File with this name already exists in <b>%1</b>.</p><p>Please change some of your tags.</p>")
				.arg(file.canonicalPath()));
		return RenameStatus::Failed;
	}

	if(force_save) {
		QFile(m_current_file).rename(new_file_path);
		m_current_file = new_file_path;
		return RenameStatus::Renamed;
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

	auto reply = renameMessageBox.exec();

	if(reply == QMessageBox::Save) {
		QFile(m_current_file).rename(new_file_path);
		m_current_file = new_file_path;
		return RenameStatus::Renamed;
	}

	if(reply == QMessageBox::Cancel) {
		return RenameStatus::Cancelled;
	}

	return RenameStatus::Failed;
}
