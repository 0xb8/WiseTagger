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
#include "util/size.h"
#include "window.h"

namespace logging_category {
	Q_LOGGING_CATEGORY(tagger, "Tagger")
}
#define pdbg qCDebug(logging_category::tagger)
#define pwarn qCWarning(logging_category::tagger)

Tagger::Tagger(QWidget *_parent) :
	QWidget(_parent)
{
	installEventFilter(_parent);
	m_picture.installEventFilter(_parent);
	m_file_queue.setNameFilter(QStringList({QStringLiteral("*.jpg"),
						QStringLiteral("*.jpeg"),
						QStringLiteral("*.png"),
						QStringLiteral("*.gif"),
						QStringLiteral("*.bmp")}));

	m_main_layout.setMargin(0);
	m_main_layout.setSpacing(0);

	m_tag_input_layout.setMargin(10);
	m_tag_input_layout.addWidget(&m_input);

	m_separator.setFrameStyle(QFrame::HLine | QFrame::Sunken);
	m_main_layout.addWidget(&m_picture);
	m_main_layout.addWidget(&m_separator);
	m_main_layout.addLayout(&m_tag_input_layout);
	setLayout(&m_main_layout);
	setAcceptDrops(true);

	setObjectName(QStringLiteral("Tagger"));
	m_picture.setObjectName(QStringLiteral("Picture"));
	m_input.setObjectName(QStringLiteral("Input"));
	m_separator.setObjectName(QStringLiteral("Separator"));

	connect(&m_input, &TagInput::textEdited,     this, &Tagger::tagsEdited);
	connect(this,     &Tagger::fileOpened,       this, &Tagger::findTagsFiles);
	connect(this,     &Tagger::fileRenamed, &m_statistics, &TaggerStatistics::fileRenamed);
	connect(this,     &Tagger::fileOpened, [this](const auto& f)
	{
		m_statistics.fileOpened(f, m_picture.imageSize());
	});
}


Tagger::~Tagger() { }

bool Tagger::openFile(const QString &filename)
{
	const QFileInfo fi(filename);
	if(!fi.isReadable() || !fi.isFile() || !fi.isAbsolute()) {
		pwarn << "invalid file path";
		return false;
	}
	m_file_queue.clear();
	m_file_queue.push(fi.absolutePath());
	m_file_queue.sort();
	m_file_queue.select(m_file_queue.find(fi.absoluteFilePath()));
	loadCurrentFile();
	return !m_file_queue.empty();
}

bool Tagger::openDir(const QString &dir)
{
	const QFileInfo fi(dir);
	if(!fi.isReadable() || !fi.isDir() || !fi.isAbsolute()) {
		pwarn << "invalid directory path";
		return false;
	}
	m_file_queue.clear();
	m_file_queue.push(dir);
	m_file_queue.sort();
	m_file_queue.select(0u);
	loadCurrentFile();
	return !m_file_queue.empty();
}

void Tagger::openFileInQueue(size_t index)
{
	m_file_queue.select(index);
	loadCurrentFile();
}

void Tagger::openSession(const QString& sfile)
{
	if(sfile.isEmpty())
		return;
	if(!m_file_queue.loadFromFile(sfile)) {
		QMessageBox::critical(this,
			tr("Load Session Failed"),
			tr("<p>Could not load session from <b>%1</b></p>").arg(sfile));
		return;
	}
	loadCurrentFile();
}

void Tagger::deleteCurrentFile()
{
	QMessageBox delete_msgbox(QMessageBox::Question, tr("Delete file?"),
		tr("<p>Are you sure you want to delete <b>%1</b> permanently?</p>"
		   "<dd><dl>File type: %2</dl>"
		   "<dl>File size: %3</dl>"
		   "<dl>Dimensions: %4 x %5</dl>"
		   "<dl>Modified: %6</dl></dd>"
		   "<p><em>This action cannot be undone!</em></p>").arg(
			currentFileName(),
			currentFileType(),
			util::size::printable(pictureSize()),
			QString::number(pictureDimensions().width()),
			QString::number(pictureDimensions().height()),
			QFileInfo(currentFile()).lastModified().toString(tr("yyyy-MM-dd hh:mm:ss", "modified date"))),
		QMessageBox::Save|QMessageBox::Cancel);
	delete_msgbox.setButtonText(QMessageBox::Save, tr("Delete"));

	const auto reply = delete_msgbox.exec();

	if(reply == QMessageBox::Save) {
		if(!m_file_queue.deleteCurrentFile()) {
			QMessageBox::warning(this,
				tr("Could not delete file"),
				tr("<p>Could not delete current file.</p>"
				   "<p>This can happen when you don\'t have write permissions to that file, "
				   "or when file has been renamed or removed by another application.</p>"
				   "<p>Next file will be opened instead.</p>"));
		}
		loadCurrentFile();
	}
}

FileQueue& Tagger::queue()
{
	return m_file_queue;
}

TaggerStatistics &Tagger::statistics()
{
	return m_statistics;
}

void Tagger::nextFile(RenameOptions options)
{
	if(fileModified() && (rename(options) == RenameStatus::Cancelled))
		return;
	m_file_queue.forward();
	loadCurrentFile();
}

void Tagger::prevFile(RenameOptions options)
{
	if(fileModified() && (rename(options) == RenameStatus::Cancelled))
		return;
	m_file_queue.backward();
	loadCurrentFile();
}


/* just load picture into tagger */
void Tagger::loadCurrentFile()
{
	bool silent = false;
	while(!loadFile(m_file_queue.currentIndex(), silent) && !m_file_queue.empty()) {
		pdbg << "erasing invalid file from queue:" << m_file_queue.current();
		m_file_queue.eraseCurrent();
		silent = true;
	}

	if(m_file_queue.empty()) {
		clear();
		return;
	}
	emit fileOpened(m_file_queue.current());
}
//------------------------------------------------------------------------------

QString Tagger::currentFile() const
{
	return m_file_queue.current();
}

QString Tagger::currentDir() const
{
	return QFileInfo(m_file_queue.current()).absolutePath();
}

QString Tagger::currentFileName() const
{
	return QFileInfo(m_file_queue.current()).fileName();
}

QString Tagger::currentFileType() const
{
	QFileInfo fi(m_file_queue.current());
	return fi.suffix().toUpper();
}

bool Tagger::fileModified() const
{
	if(m_file_queue.empty()) // NOTE: to avoid FileQueue::current() returning invalid reference.
		return false;

	return m_input.text() != QFileInfo(m_file_queue.current()).completeBaseName();
}

QSize Tagger::pictureDimensions() const
{
	return m_picture.imageSize();
}


qint64 Tagger::pictureSize() const
{
	return QFileInfo(m_file_queue.current()).size();
}

QString Tagger::postURL() const
{
	return m_input.postURL();
}

//------------------------------------------------------------------------------
void Tagger::setInputVisible(bool visible)
{
	m_input.setVisible(visible);
	m_separator.setVisible(visible);
	if(visible) {
		m_tag_input_layout.setMargin(10);
	} else {
		m_tag_input_layout.setMargin(0);
	}
}

void Tagger::reloadTags()
{
	m_input.loadTagFiles(m_current_tag_files);
	m_input.clearTagState();
}

void Tagger::updateSettings()
{
	m_input.updateSettings();
}

void Tagger::findTagsFiles()
{
	if(m_file_queue.empty())
		return;

	const auto c = currentDir();
	if(c == m_previous_dir)
		return;

	m_previous_dir = c;
	QFileInfo fi(m_file_queue.current());

	const QSettings settings;
	const auto tagsfile = settings.value(QStringLiteral("tags/normal"),
					     QStringLiteral("*.tags.txt")).toString();

	const auto override = settings.value(QStringLiteral("tags/override"),
					     QStringLiteral("*.tags!.txt")).toString();

	QString parent_dir, errordirs;

	const int path_capacity = 256;
	const int errors_capacity = 4 * path_capacity;

	parent_dir.reserve(path_capacity);
	errordirs.reserve(errors_capacity);

	errordirs += QStringLiteral("<li>");

	int max_height = 10;
	while(max_height --> 0) {
		parent_dir = fi.absolutePath();
		errordirs += parent_dir;
		errordirs += QStringLiteral("</li><li>");

		auto cd = fi.dir();
		auto list_override = cd.entryInfoList({override});
		if(!list_override.isEmpty()) {
			for(const auto& f : list_override) {
				m_current_tag_files.push_back(f.absoluteFilePath());
				pdbg << "found override:" << f.fileName();
			}
			break;
		}

		auto list_tags = cd.entryInfoList({tagsfile});
		for(const auto& f : list_tags) {
			m_current_tag_files.push_back(f.absoluteFilePath());
			pdbg << "found tag file:" << f.fileName();
		}

		// support old files
		if(list_override.isEmpty() && list_tags.isEmpty()) {
			auto legacy_override = parent_dir, legacy_tags = parent_dir;
			legacy_override += QChar('/');
			legacy_tags += QChar('/');
			std::copy(override.begin() + 2, override.end(), std::back_inserter(legacy_override));
			std::copy(tagsfile.begin() + 2, tagsfile.end(), std::back_inserter(legacy_tags));

			if(QFile::exists(legacy_override)) {
				m_current_tag_files.push_back(legacy_override);
				pdbg << "found legacy override:" << legacy_override;
				break;
			}
			if(QFile::exists(legacy_tags)) {
				m_current_tag_files.push_back(legacy_tags);
				pdbg << "found legacy tags:" << legacy_tags;
			}
		}

		if(fi.absoluteDir().isRoot())
			break;

		fi.setFile(parent_dir);
	}

	m_current_tag_files.removeDuplicates();

	if(m_current_tag_files.isEmpty()) {
		auto last_resort = qApp->applicationDirPath();
		last_resort += QStringLiteral("/tags.txt");

		errordirs += qApp->applicationDirPath();
		errordirs += QStringLiteral("</li>");

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
				"<p><a href=\"https://bitbucket.org/catgirl/wisetagger/overview\">"
				"Appending and overriding tag files documentation"
				"</a></p>").arg(tagsfile, override, errordirs));
			mbox.setIcon(QMessageBox::Warning);
			mbox.exec();
		}
	}

	std::reverse(m_current_tag_files.begin(), m_current_tag_files.end());
	reloadTags();
}

void Tagger::clear()
{
	m_file_queue.clear();
	m_picture.clear();
	m_input.clear();
}

bool Tagger::loadFile(size_t index, bool silent)
{
	const auto filename = m_file_queue.select(index);
	if(filename.size() == 0)
		return false;

	const QFileInfo f(filename);
	const QDir fd(f.absolutePath());

	if(!fd.exists()) {
		if(!silent) {
			QMessageBox::critical(this,
			tr("Error opening file"),
			tr("<p>Directory <b>%1</b> does not exist anymore.</p>"
			   "<p>File from another directory will be opened instead.</p>")
				.arg(fd.absolutePath()));
		}
		return false;
	}

	if(!f.exists()) {
		if(!silent) {
			QMessageBox::critical(this,
			tr("Error opening file"),
			tr("<p>File <b>%1</b> does not exist anymore.</p>"
			   "<p>Next file will be opened instead.</p>")
				.arg(f.fileName()));
		}
		return false;
	}

	if(!m_picture.loadPicture(f.absoluteFilePath())) {
		QMessageBox::critical(this,
			tr("Error opening image"),
			tr("<p>Could not open <b>%1</b></p>"
			   "<p>File format is not supported or file corrupted.</p>")
				.arg(f.fileName()));
		return false;
	}

	m_input.setText(f.completeBaseName());
	findTagsFiles();
	m_picture.setFocus();
	return true;
}

void Tagger::updateNewTagsCounts() {
	const auto new_tags = m_input.getAddedTags(true);
	static int overall_count = 0;
	bool emitting = false;

	for(const auto& t : new_tags) {
		auto it = m_new_tag_counts.find(t);
		if(it != m_new_tag_counts.end()) {
			++it->second;
			if(it->second == 5)
				emitting = true;

		} else {
			m_new_tag_counts.insert(std::make_pair(t, 1u));
		}
		++overall_count;
	}

	if(overall_count >= std::min(8.0, 2*std::log2(m_file_queue.size()))) {
		emitting = true;
	}

	if(emitting) {
		QStringList tags;
		for(const auto& t : m_new_tag_counts) {
			tags.push_back(t.first);
		}
		std::sort(tags.begin(), tags.end(), [this](const QString&a, const QString&b) {
			const unsigned numa = m_new_tag_counts[a];
			const unsigned numb = m_new_tag_counts[b];
			if(numa == numb)
				return a < b;
			return numa > numb;
		});
		emit newTagsAdded(tags);
		overall_count = 0;
	}
}

Tagger::RenameStatus Tagger::rename(RenameOptions options)
{
	const QFileInfo file(m_file_queue.current());
	QString new_file_path;

	m_input.fixTags();

	/* Make new file path from input text */
	new_file_path += m_input.text();
	new_file_path += QChar('.');
	new_file_path += file.suffix();
	new_file_path = QFileInfo(QDir(file.canonicalPath()), new_file_path).filePath();

	if(new_file_path == m_file_queue.current() || m_input.text().isEmpty())
		return RenameStatus::Failed;

	/* Check for possible filename conflict */
	if(QFileInfo::exists(new_file_path)) {
		QMessageBox::critical(this,
			tr("Cannot rename file"),
			tr("<p>Cannot rename file <b>%1</b></p>"
			   "<p>File with this name already exists in <b>%2</b></p>"
			   "<p>Please change some of your tags.</p>")
				.arg(file.fileName(), file.canonicalPath()));
		return RenameStatus::Cancelled;
	}

	/* Show save dialog */
	QMessageBox renameMessageBox(QMessageBox::Question,
		tr("Rename file?"),
		tr("Rename <b>%1</b>?").arg(file.completeBaseName()),
		QMessageBox::Save|QMessageBox::Discard);

	renameMessageBox.setButtonText(QMessageBox::Save, tr("Rename"));

	if(!options.testFlag(RenameOption::HideCancelButton)) { // FIXME: is this even needed?
		renameMessageBox.addButton(QMessageBox::Cancel);
	}
	renameMessageBox.setButtonText(QMessageBox::Discard, tr("Discard"));

	int reply;
	if(options.testFlag(RenameOption::ForceRename) || (reply = renameMessageBox.exec()) == QMessageBox::Save ) {
		if(!m_file_queue.renameCurrentFile(new_file_path)) {
			QMessageBox::warning(this,
				tr("Could not rename file"),
				tr("<p>Could not rename <b>%1</b></p>"
				   "<p>File may have been renamed or removed by another application.</p>").arg(file.fileName()));
			return RenameStatus::Failed;
		}
		updateNewTagsCounts();
		emit fileRenamed(m_input.text());
		return RenameStatus::Renamed;
	}

	if(reply == QMessageBox::Cancel) {
		return RenameStatus::Cancelled;
	}

	return RenameStatus::Failed;
}
