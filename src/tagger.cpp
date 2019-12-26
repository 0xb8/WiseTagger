/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "tagger.h"
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QInputDialog>
#include <algorithm>
#include <cmath>
#include "global_enums.h"
#include "util/size.h"
#include "util/misc.h"
#include "util/open_graphical_shell.h"
#include "window.h"
#include "statistics.h"

namespace logging_category {
	Q_LOGGING_CATEGORY(tagger, "Tagger")
}
#define pdbg qCDebug(logging_category::tagger)
#define pwarn qCWarning(logging_category::tagger)
#define ESCAPE_HIDE_TIMEOUT_MS 600

Tagger::Tagger(QWidget *_parent) :
	QWidget(_parent)
{
	installEventFilter(_parent);
	m_picture.installEventFilter(_parent);
	m_file_queue.setNameFilter(util::supported_image_formats_namefilter());

	m_main_layout.setMargin(0);
	m_main_layout.setSpacing(0);

	m_tag_input_layout.addWidget(&m_input);

	m_separator.setFrameStyle(QFrame::HLine | QFrame::Sunken);
	m_main_layout.addWidget(&m_picture);
	m_main_layout.addWidget(&m_separator);
	m_main_layout.addLayout(&m_tag_input_layout);
	setLayout(&m_main_layout);
	setAcceptDrops(true);

	setObjectName(QStringLiteral("Tagger"));
	m_input.setObjectName(QStringLiteral("Input"));
	m_separator.setObjectName(QStringLiteral("Separator"));

	connect(&m_input, &TagInput::textEdited, this, &Tagger::tagsEdited);
	connect(&m_input, &TagInput::parseError, this, &Tagger::parseError);
	connect(this, &Tagger::fileRenamed, &TaggerStatistics::instance(), &TaggerStatistics::fileRenamed);
	connect(this, &Tagger::fileOpened, this, [this](const auto& file)
	{
		m_fetcher.abort();
		TaggerStatistics::instance().fileOpened(file, m_picture.mediaSize());
	});
	connect(&m_fetcher, &TagFetcher::ready, this, &Tagger::tagsFetched);
	updateSettings();
	setFocusPolicy(Qt::StrongFocus);
}

void Tagger::clear()
{
	m_file_queue.clear();
	m_picture.clear();
	m_picture.cache.clear();
	m_input.clear();
}

bool Tagger::open(const QString& filename)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	bool res     = openDir(filename);
	if(!res) res = openSession(filename);
	if(!res) res = openFile(filename);
	return res;
}

bool Tagger::openFile(const QString &filename)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	const QFileInfo fi(filename);
	if(!fi.isReadable() || !fi.isFile()) {
		return false;
	}
	m_file_queue.clear();
	m_file_queue.push(fi.absolutePath());
	m_file_queue.sort();
	m_file_queue.select(m_file_queue.find(fi.absoluteFilePath()));
	m_picture.cache.clear();
	return loadCurrentFile();
}

bool Tagger::openDir(const QString &dir)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	const QFileInfo fi(dir);
	if(!fi.isReadable() || !fi.isDir()) {
		return false;
	}
	m_file_queue.clear();
	m_file_queue.push(dir);
	m_file_queue.sort();
	m_file_queue.select(0u); // NOTE: must select after sorting, otherwise selects first file in directory order
	m_picture.cache.clear();
	return loadCurrentFile();
}

bool Tagger::openSession(const QString& sfile)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	// NOTE: to prevent error message when opening normal file or directory
	if(!FileQueue::checkSessionFileSuffix(sfile)) return false;

	if(!m_file_queue.loadFromFile(sfile)) {
		QMessageBox::critical(this,
			tr("Load Session Failed"),
			tr("<p>Could not load session from <b>%1</b></p>").arg(sfile));
		return false;
	}
	m_picture.cache.clear();

	bool res = loadCurrentFile();
	if(res) {
		emit sessionOpened(sfile);
	}
	return res;
}

void Tagger::nextFile(RenameOptions options)
{
	if(rename(options) == RenameStatus::Cancelled)
		return;

	m_file_queue.forward();
	loadCurrentFile();
}

void Tagger::prevFile(RenameOptions options)
{
	if(rename(options) == RenameStatus::Cancelled)
		return;

	m_file_queue.backward();
	loadCurrentFile();
}

bool Tagger::openFileInQueue(size_t index)
{
	m_file_queue.select(index);
	return loadCurrentFile();
}

void Tagger::deleteCurrentFile()
{
	QMessageBox delete_msgbox(QMessageBox::Question, tr("Delete file?"),
		tr("<h3 style=\"font-weight: normal;\">Are you sure you want to delete <u>%1</u> permanently?</h3>"
		   "<dd><dl>File type: %2</dl>"
		   "<dl>File size: %3</dl>"
		   "<dl>Dimensions: %4 x %5</dl>"
		   "<dl>Modified: %6</dl></dd>"
		   "<p><em>This action cannot be undone!</em></p>").arg(
			currentFileName(),
			currentFileType(),
			util::size::printable(mediaFileSize()),
			QString::number(mediaDimensions().width()),
			QString::number(mediaDimensions().height()),
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

void Tagger::fixTags()
{
	m_input.fixTags();
}


void Tagger::fetchTags()
{
	auto url = m_input.postTagsApiURL();
	if (url.size() == 0)
		return;

	m_fetcher.fetch_tags(url);
}

void Tagger::tagsFetched(QString url, QString tags)
{
	util::replace_special(tags);

	// Current file might have already changed since reply came
	if (url == m_input.postTagsApiURL()) {
		auto current_tags = m_input.tags();
		if (current_tags != tags) {
			// Ask user what to do with tags
			QMessageBox mbox(QMessageBox::Question,
			                 tr("Fetched tags mismatch"),
			                 tr("<p>Imageboard tags do not match current tags:"
			                    "<br/><pre>%1</pre></p>"
			                    "<p>Please choose what to do:</p>").arg(tags),
			                 QMessageBox::Save|QMessageBox::SaveAll);

			mbox.addButton(QMessageBox::Cancel);
			mbox.setButtonText(QMessageBox::Save, tr("Use just imageboard tags"));
			mbox.setButtonText(QMessageBox::SaveAll, tr("Merge tags"));
			int res = mbox.exec();

			if (res == QMessageBox::Save) {
				m_input.setTags(tags);
			}
			if (res == QMessageBox::SaveAll) {
				current_tags.append(' ');
				current_tags.append(tags);
				m_input.setTags(current_tags);
			}
		}
	}
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

bool Tagger::isEmpty() const
{
	return m_file_queue.empty();
}

bool Tagger::hasTagFile() const
{
	return m_input.hasTagFile();
}

QSize Tagger::mediaDimensions() const
{
	return m_picture.mediaSize();
}

size_t Tagger::mediaFileSize() const
{
	return QFileInfo(m_file_queue.current()).size();
}

QString Tagger::postURL() const
{
	return m_input.postURL();
}

FileQueue& Tagger::queue()
{
	return m_file_queue;
}

TagFetcher & Tagger::tag_fetcher()
{
	return m_fetcher;
}

bool Tagger::canExit(Tagger::RenameStatus status)
{
	if(status == RenameStatus::Cancelled || status == RenameStatus::Failed)
		return false;
	return true;
}

//------------------------------------------------------------------------------

void Tagger::updateSettings()
{
	QSettings s;
	auto view_mode = s.value(QStringLiteral("window/view-mode")).value<ViewMode>();
	if(view_mode == ViewMode::Minimal) {
		m_tag_input_layout.setMargin(0);
		m_separator.hide();
	}
	if(view_mode == ViewMode::Normal) {
		m_tag_input_layout.setMargin(m_tag_input_layout_margin);
		m_separator.show();
	}
	m_input.updateSettings();
	m_picture.cache.setMemoryLimitKiB(s.value(QStringLiteral("performance/pixmap_cache_size"), 0ull).toULongLong() * 1024);
	m_picture.cache.setMaxConcurrentTasks(s.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt() * 2);
}

void Tagger::keyPressEvent(QKeyEvent * e)
{
	if (e->key() == Qt::Key_Escape) {
		// escape already been pressed once
		if (m_hide_request_timer.isActive()) {
			m_hide_request_timer.stop();
			emit hideRequested();
		} else {
			// start timer to detect second escape press
			m_hide_request_timer.start(ESCAPE_HIDE_TIMEOUT_MS, this);
		}
	}
	QWidget::keyPressEvent(e);
}

void Tagger::timerEvent(QTimerEvent*)
{
	// timer is single shot
	m_hide_request_timer.stop();
}

void Tagger::focusInEvent(QFocusEvent * e)
{
	if (e->reason() == Qt::OtherFocusReason) {
		// we've been focused by tag input escape or enter keypress
		m_hide_request_timer.start(ESCAPE_HIDE_TIMEOUT_MS, this);
	}
	QWidget::focusInEvent(e);
}

void Tagger::setInputVisible(bool visible)
{
	m_input.setVisible(visible);

	QSettings s;
	auto view_mode = s.value(QStringLiteral("window/view-mode")).value<ViewMode>();
	if(view_mode == ViewMode::Minimal)
		return; // NOTE: no need to update following properties because they've been taken care of already

	m_separator.setVisible(visible);
	if(visible) {
		m_tag_input_layout.setMargin(m_tag_input_layout_margin);
	} else {
		m_tag_input_layout.setMargin(0);
	}
}

//------------------------------------------------------------------------------

void Tagger::reloadTags()
{
	findTagsFiles(true);
}

void Tagger::reloadTagsContents()
{
	QByteArray data;
	for(const auto& filename : qAsConst(m_current_tag_files)) {
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
			QMessageBox::warning(this,
				tr("Error opening tag file"),
				tr("<p>Could not open <b>%1</b></p>"
				   "<p>File may have been renamed or removed by another application.</p>").arg(filename));
			continue;
		}
		data.push_back(file.readAll());
		data.push_back('\n');
	}

	data.push_back(m_temp_tags.toUtf8());

	m_input.loadTagData(data);
	m_input.clearTagState();
}

void Tagger::openTagFilesInEditor()
{
	for(const auto& file : qAsConst(m_current_tag_files)) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(file));
	}
}

void Tagger::openTempTagFileEditor()
{
	bool ok;
	QString text = QInputDialog::getMultiLineText(this,
	                                              tr("Edit temporary tags"),
	                                              tr("Temporary tags:"),
	                                              m_temp_tags,
	                                              &ok);

	if(ok) {
		m_temp_tags = text;
		reloadTagsContents();
	}
}

void Tagger::openTagFilesInShell()
{
	for(const auto& file : qAsConst(m_current_tag_files)) {
		util::open_file_in_gui_shell(file);
	}
}

void Tagger::findTagsFiles(bool force)
{
	if(m_file_queue.empty())
		return;

	const auto c = currentDir();
	if(c == m_previous_dir && !force)
		return;

	m_previous_dir = c;
	QFileInfo fi(m_file_queue.current());

	const QSettings settings;
	const auto tagsfile = settings.value(QStringLiteral("tags/normal"),
					     QStringLiteral("*.tags.txt")).toString();

	const auto override = settings.value(QStringLiteral("tags/override"),
					     QStringLiteral("*.tags!.txt")).toString();

	QList<QDir> search_dirs;
	QDir current_dir = fi.absoluteDir();


	int max_height = 100; // NOTE: to avoid infinite loop with symlinks etc.
	do {
		search_dirs.append(current_dir);
	} while(max_height --> 0 && current_dir.cdUp());


	for(const auto& loc : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
	{
		search_dirs.append(QDir{loc});
	}


	auto add_tag_files = [this](const QFileInfoList& list)
	{
		for(const auto& f : qAsConst(list)) {
			m_current_tag_files.push_back(f.absoluteFilePath());
		}
	};

	m_current_tag_files.clear();

	QStringList search_paths_list;
	search_paths_list.reserve(search_dirs.size());

	for(const auto& dir : qAsConst(search_dirs)) {
		search_paths_list.push_back(dir.path());
		if(!dir.exists()) continue;

		auto filter = QDir::Files | QDir::Hidden;
		auto sort_by = QDir::Name;
		auto list_override = dir.entryInfoList({override}, filter, sort_by);
		auto list_normal   = dir.entryInfoList({tagsfile}, filter, sort_by);

		add_tag_files(list_override);

		if(!list_override.isEmpty())
			break;

		add_tag_files(list_normal);

		// support old files
		if(list_override.isEmpty() && list_normal.isEmpty()) {

			auto legacy_override_list = dir.entryInfoList({override.mid(2)});
			auto legacy_normal_list   = dir.entryInfoList({tagsfile.mid(2)});

			add_tag_files(legacy_override_list);
			if(!legacy_override_list.isEmpty())
				break;

			add_tag_files(legacy_normal_list);
			if(!legacy_normal_list.isEmpty())
				break;
		}
	}

	m_current_tag_files.removeDuplicates();

	pdbg << m_current_tag_files;

	if(Q_LIKELY(!m_current_tag_files.isEmpty())) {
		std::reverse(m_current_tag_files.begin(), m_current_tag_files.end());
		m_fs_watcher = std::make_unique<QFileSystemWatcher>(nullptr);
		m_fs_watcher->addPaths(m_current_tag_files);
		connect(m_fs_watcher.get(), &QFileSystemWatcher::fileChanged, this, [this](const auto& f)
		{
			this->reloadTags();
			pdbg << "reloaded tags due to changed file:" << f;
			emit this->tagFileChanged();
		});
		reloadTagsContents();
	} else {
		emit tagFilesNotFound(tagsfile, override, search_paths_list);
	}
}

void Tagger::updateNewTagsCounts()
{
	const auto new_tags = m_input.getAddedTags(true);
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
		++m_overall_new_tag_counts;
	}

	if(m_overall_new_tag_counts >= std::min(8.0, 2*std::log2(m_file_queue.size()))) {
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
		m_overall_new_tag_counts = 0;
	}
}

//------------------------------------------------------------------------------

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

	if(!m_picture.loadMedia(f.absoluteFilePath())) {
		QMessageBox::critical(this,
			tr("Error opening media"),
			tr("<p>Could not open <b>%1</b></p>"
			   "<p>File format is not supported or file corrupted.</p>")
				.arg(f.fileName()));
		return false;
	}

	m_input.setText(f.completeBaseName());
	findTagsFiles();
	setFocus(Qt::TabFocusReason);
	return true;
}

/* just load picture into tagger */
bool Tagger::loadCurrentFile()
{
	bool silent = false;
	while(!loadFile(m_file_queue.currentIndex(), silent) && !m_file_queue.empty()) {
		pdbg << "erasing invalid file from queue:" << m_file_queue.current();
		m_file_queue.eraseCurrent();
		silent = true;
	}

	if(m_file_queue.empty()) {
		clear();
		return false;
	}
	emit fileOpened(m_file_queue.current());
	findTagsFiles();

	if(m_file_queue.size() < 2)
		return true;

	QSettings s;
	if(!s.value(QStringLiteral("performance/pixmap_precache_enabled"), false).toBool())
		return true;

	int preloadcount = abs(s.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt());
	preloadcount = std::max(std::min(preloadcount, 16), 1);
	for(int i = -preloadcount; i <= preloadcount; ++i)
	{
		if(i == 0)
			continue;
		auto idx = ssize_t(m_file_queue.currentIndex())+i;
		m_picture.cache.addFile(m_file_queue.nth(idx), m_picture.size());
	}

	return true;
}

Tagger::RenameStatus Tagger::rename(RenameOptions options)
{
	if(!fileModified())
		return RenameStatus::NotModified;

	const QFileInfo file(m_file_queue.current());
	QString new_file_path;

	m_input.fixTags();
	auto filename = m_input.text();
	filename.append('.');
	filename.append(file.suffix());

	// check if the filename is too long for most modern filesystems
	const int path_limit = 255;
	if (filename.size() > path_limit) {
		QMessageBox::warning(this,
			tr("Could not rename file"),
			tr("<p>Could not rename <b>%1</b></p>"
			   "<p>File name is too long: <b>%2</b> characters, but maximum allowed file name length is %3 characters.</p>"
		           "<p>Please change some of your tags.</p>")
				.arg(file.fileName())
				.arg(filename.size())
				.arg(path_limit));
		return RenameStatus::Cancelled;
	}

	/* Make new file path from input text */
	new_file_path.append(filename);

#ifdef Q_OS_WIN
	auto tpos = std::remove_if(new_file_path.begin(), new_file_path.end(), [](QChar c)
	{
		return c < 31;
	});
	if(tpos != new_file_path.end()) {
		pdbg << "Removing reserved characters from filename...";
		new_file_path.truncate(std::distance(new_file_path.begin(), tpos));
	}
#endif

	new_file_path = QFileInfo(QDir(file.canonicalPath()), new_file_path).filePath();

	if(new_file_path == m_file_queue.current() || m_input.text().isEmpty())
		return RenameStatus::Failed;

	/* Show save dialog */
	QMessageBox renameMessageBox(QMessageBox::Question,
		tr("Rename file?"),
		tr("Rename <b>%1</b>?").arg(file.completeBaseName()),
		QMessageBox::Save|QMessageBox::Discard);

	renameMessageBox.addButton(QMessageBox::Cancel);
	renameMessageBox.setButtonText(QMessageBox::Save, tr("Rename"));
	renameMessageBox.setButtonText(QMessageBox::Discard, tr("Discard"));

	QSettings settings;
	int reply;
	if(options.testFlag(RenameOption::ForceRename) || (reply = renameMessageBox.exec()) == QMessageBox::Save ) {

		auto result = m_file_queue.renameCurrentFile(new_file_path);

		switch (result) {
		case FileQueue::RenameResult::SourceFileMissing:
		case FileQueue::RenameResult::GenericFailure:

			QMessageBox::warning(this,
				tr("Could not rename file"),
				tr("<p>Could not rename <b>%1</b></p>"
				   "<p>File may have been renamed or removed by another application, "
				   "file with this name may already exist in the current directory or exceed the character limit.</p>")
					.arg(file.fileName()));
			return RenameStatus::Failed;

		case FileQueue::RenameResult::TargetFileExists:

			QMessageBox::critical(this,
				tr("Cannot rename file"),
				tr("<p>Cannot rename <b>%1</b></p>"
				   "<p>File with this name already exists in <b>%2</b></p>"
				   "<p>Please change some of your tags.</p>")
					.arg(file.fileName(), file.canonicalPath()));
			return RenameStatus::Cancelled;

		case FileQueue::RenameResult::Success:
			if(settings.value(QStringLiteral("track-added-tags"), true).toBool()) {
				updateNewTagsCounts();
			}
			emit fileRenamed(m_input.text());
			return RenameStatus::Renamed;

		}
	}

	if(reply == QMessageBox::Cancel) {
		return RenameStatus::Cancelled;
	}

	if(reply == QMessageBox::Discard) {
		// NOTE: restore to initial state to prevent multiple rename dialogs
		m_input.setText(file.completeBaseName());
		return RenameStatus::NotModified;
	}

	return RenameStatus::Failed;
}
