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

#ifdef Q_OS_WIN
	extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#else
	static int qt_ntfs_permission_lookup;
#endif

Tagger::Tagger(QWidget *_parent) :
	QWidget(_parent)
{
	installEventFilter(_parent);
	m_picture.installEventFilter(_parent);
	m_file_queue.setNameFilter(util::supported_image_formats_namefilter() +
	                           util::supported_video_formats_namefilter());

	m_main_layout.setMargin(0);
	m_main_layout.setSpacing(0);

	m_tag_input_layout.addWidget(&m_input);

	m_separator.setFrameStyle(QFrame::HLine | QFrame::Sunken);
	m_main_layout.addWidget(&m_picture);
	m_main_layout.addWidget(&m_video);
	m_main_layout.addWidget(&m_separator);
	m_main_layout.addLayout(&m_tag_input_layout);
	setLayout(&m_main_layout);
	setAcceptDrops(true);

	m_playlist.setPlaybackMode(QMediaPlaylist::PlaybackMode::CurrentItemInLoop);
	m_player.setPlaylist(&m_playlist);
	m_player.setVideoOutput(&m_video);

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
	connect(&m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, [this](QMediaPlayer::Error) {
		hideVideo();
		QMessageBox::critical(this,
			tr("Error opening media"),
			tr("<p>Could not open <b>%1</b></p>"
			   "<p>File format is not supported or file corrupted.</p>"
			   "<p>Media player error: %2</p>")
				.arg(currentFileName()).arg(m_player.errorString()));
	});
	connect(&m_player, &QMediaPlayer::metaDataAvailableChanged, this, [this](bool){
		emit fileOpened(currentFile());
	});


	updateSettings();
	setFocusPolicy(Qt::ClickFocus);
	clear();
}

void Tagger::clear()
{
	hideVideo();
	m_file_queue.clear();
	m_picture.clear();
	m_picture.cache.clear();
	m_input.clear();
	m_input.clearTagData();
	m_input.clearTagEditState();
	m_input.setEnabled(true);
	emit cleared();
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
		if (mediaIsVideo()) {
			hideVideo();
		}
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
	Q_ASSERT(fileRenameable());
	m_fetcher.fetch_tags(currentFile(), m_input.postTagsApiURL());
}

void Tagger::tagsFetched(QString file, QString tags)
{
	// fetched usually have only single space separator
	int overall_tags_count = tags.count(' ') + (tags.isEmpty() ? 0 : 1);
	int processed_tags_count = 0;

	// Current file might have already changed since reply came
	if (currentFile() == file) {
		TagEditState state;
		auto options = TagParser::FixOptions::from_settings();
		options.sort = true;

		// Autofix imageboard tags before comparing and assigning
		util::replace_special(tags);
		auto fixed_tags_list = m_input.tag_parser().fixTags(state, tags, options);
		tags = fixed_tags_list.join(' ');
		processed_tags_count = fixed_tags_list.size();

		util::replace_special(tags);
		auto current_tags = m_input.tags();

		if (current_tags != tags) {

			if (!util::is_hex_string(current_tags)) {

				auto current_list = m_input.tags_list();
				// sort current tags in case they were being edited and unsorted.
				// imageboard tags are already sorted.
				current_list.sort();

				QString added_tags_str, removed_tags_str;
				getTagDifference(current_list, fixed_tags_list, added_tags_str, removed_tags_str, true);

				// Ask user what to do with tags
				QMessageBox mbox(QMessageBox::Question,
				                 tr("Fetched tags mismatch"),
				                 tr("<p>Imageboard tags do not match current tags:"
				                    "<p style=\"margin-left: 1em; line-height: 130%;\">"
				                    "<code>%1</code></p></p>"
				                    "%2%3"
				                    "<p>Please choose what to do:</p>").arg(fixed_tags_list.join(' '))
				                                                       .arg(added_tags_str)
				                                                       .arg(removed_tags_str),
				                 QMessageBox::Save|QMessageBox::SaveAll);

				mbox.addButton(QMessageBox::Cancel);
				mbox.setButtonText(QMessageBox::Save, tr("Use only imageboard tags"));
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
			} else {
				// if there was only hash filename to begin with,
				// just use the imageboard tags without asking
				m_input.setText(tags);
			}
		}
	}

	TaggerStatistics::instance().tagsFetched(overall_tags_count, processed_tags_count);
}

void Tagger::getTagDifference(QStringList current_list,
                              QStringList fixed_tags_list,
                              QString & added_tags_str,
                              QString & removed_tags_str,
                              bool show_merge_hint)
{

	QStringList tags_removed; // find tags that are in current tags but not in imageboard tags
	std::set_difference(current_list.begin(), current_list.end(),
	                    fixed_tags_list.begin(), fixed_tags_list.end(),
	                    std::back_inserter(tags_removed));

	QStringList tags_added; // find tags that are in imageboard tags but not in current tags
	std::set_difference(fixed_tags_list.begin(), fixed_tags_list.end(),
	                    current_list.begin(), current_list.end(),
	                    std::back_inserter(tags_added));

	// set of all known tags
	const auto all_tags_set = QSet<QString>::fromList(m_input.tag_parser().getAllTags());
	const auto& parser = m_input.tag_parser();

	// make known tag bold
	auto mark_existing_tags = [&all_tags_set, &parser](const auto& tag)
	{
		if (all_tags_set.contains(tag)) {
			return QStringLiteral("<b>%1</b>").arg(tag);
		}
		if (parser.isTagRemoved(tag)) {
			return QStringLiteral("<s>%1</s>").arg(tag);
		}
		auto replacement = parser.getReplacement(tag);
		if (!replacement.isEmpty()) {
			return QStringLiteral("<i>%1</i>").arg(tag);
		}
		return tag;
	};

	// make all known tags bold
	std::transform(fixed_tags_list.begin(), fixed_tags_list.end(), fixed_tags_list.begin(), mark_existing_tags);
	std::transform(tags_added.begin(), tags_added.end(), tags_added.begin(), mark_existing_tags);
	std::transform(tags_removed.begin(), tags_removed.end(), tags_removed.begin(), mark_existing_tags);

	if (!tags_added.empty()) {
		added_tags_str = QStringLiteral("<p style=\"margin-top: 1.6em;\">");
		added_tags_str.append(tr("Tags that will be added:"));
		added_tags_str.append(QStringLiteral("<p style=\"margin-left: 1em; line-height: 130%;\">"
		                                     "<code style=\"color:green;\">%1</code></p></p>").arg(tags_added.join(' ')));
	}
	if (!tags_removed.empty()) {
		removed_tags_str = QStringLiteral("<p style=\"margin-top: 1.6em;\">");
		removed_tags_str.append(tr("Tags that will be removed:"));
		if (show_merge_hint) {
			removed_tags_str.append(QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;<small>("));
			removed_tags_str.append(tr("press <i>Merge tags</i> to keep"));
			removed_tags_str.append(QStringLiteral(")</small>"));
		}
		removed_tags_str.append(QStringLiteral("<p style=\"margin-left: 1em; line-height: 130%;\">"
		                                       "<code style=\"color:red;\">%2</code></p></p>").arg(tags_removed.join(' ')));
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

QDateTime Tagger::currentFileLastModified() const
{
	QFileInfo fi(m_file_queue.current());
	return fi.lastModified();
}

bool Tagger::fileModified() const
{
	if(m_file_queue.empty()) // NOTE: to avoid FileQueue::current() returning invalid reference.
		return false;

	return m_input.text() != QFileInfo(m_file_queue.current()).completeBaseName();
}

bool Tagger::fileRenameable() const
{
	return m_input.isEnabled();
}

bool Tagger::isEmpty() const
{
	return m_file_queue.empty();
}

bool Tagger::hasTagFile() const
{
	return m_input.hasTagFile();
}

bool Tagger::mediaIsVideo() const
{
	return !m_playlist.isEmpty();
}

bool Tagger::mediaIsAnimatedImage() const
{
	return m_picture.movie() != nullptr;
}

bool Tagger::mediaIsPlaying() const
{
	if (mediaIsVideo()) {
		return m_player.state() == QMediaPlayer::State::PlayingState;
	}
	if (mediaIsAnimatedImage()) {
		return m_picture.movie()->state() == QMovie::MovieState::Running;
	}
	return false;
}

QSize Tagger::mediaDimensions() const
{
	if (mediaIsVideo()) {
		if (m_player.isMetaDataAvailable()) {
			return m_player.metaData("Resolution").toSize();
		}
	}

	return m_picture.mediaSize();
}

float Tagger::mediaFramerate() const
{
	if (mediaIsVideo()) {
		if (m_player.isMetaDataAvailable()) {
			return m_player.metaData("VideoFrameRate").toFloat();
		}
	}

	if (mediaIsAnimatedImage()) {
		auto delay = m_picture.movie()->nextFrameDelay();
		if (delay > 0.0f)
			return 1000.0f / delay;
	}

	return 0.0f;
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
	m_picture.cache.setMemoryLimitKiB(s.value(QStringLiteral("performance/pixmap_cache_size"), 0ull).toULongLong() * 1024);
	m_picture.cache.setMaxConcurrentTasks(s.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt() * 2);
}

void Tagger::pauseMedia()
{
	if (mediaIsVideo())
		m_player.pause();

	if (mediaIsAnimatedImage())
		m_picture.movie()->setPaused(true);
}

void Tagger::setMediaPlaying(bool playing)
{
	if (playing)
		playMedia();
	else
		pauseMedia();
}

void Tagger::playMedia()
{
	if (mediaIsVideo()) {
		m_player.setMuted(m_media_muted);
		m_player.play();
	}

	if (mediaIsAnimatedImage()) {
		m_picture.movie()->setPaused(false);
	}
}

void Tagger::setMediaMuted(bool muted)
{
	if (mediaIsVideo())
		m_player.setMuted(muted);

	m_media_muted = muted;
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

void Tagger::setViewMode(ViewMode view_mode)
{
	if(view_mode == ViewMode::Minimal) {
		m_tag_input_layout.setMargin(0);
		m_separator.hide();
	}
	if(view_mode == ViewMode::Normal) {
		m_tag_input_layout.setMargin(m_tag_input_layout_margin);
		m_separator.show();
	}
	m_input.setViewMode(view_mode);
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
	m_input.clearTagEditState();
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
	m_current_tag_files.clear();
	m_input.clearTagData();
	m_input.clearTagEditState();

	if (!fileRenameable())
		return;

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

	if (QDir::match(util::supported_video_formats_namefilter(), f.fileName())) {

		if (!loadVideo(f))
			return false;

	} else {
		hideVideo();
		if(!m_picture.loadMedia(f.absoluteFilePath())) {
			QMessageBox::critical(this,
				tr("Error opening media"),
				tr("<p>Could not open <b>%1</b></p>"
				   "<p>File format is not supported or file corrupted.</p>")
					.arg(f.fileName()));
			return false;
		}
	}

	// check if user has write permission for this directory
	QFileInfo dir_fi(f.absolutePath());
	++qt_ntfs_permission_lookup;
	bool has_write_permission = dir_fi.isWritable();
	--qt_ntfs_permission_lookup;

	m_input.setText(f.completeBaseName());
	m_input.setToolTip(has_write_permission ? QString() : tr("Renaming disabled: User has no write permission in this directory."));
	m_input.setEnabled(has_write_permission);

	// remember original tags so we can show the difference when renaming
	m_original_tags = m_input.tags_list();
	m_original_tags.sort();
	findTagsFiles();
	setFocus(Qt::MouseFocusReason);
	return true;
}

bool Tagger::loadVideo(const QFileInfo & file)
{
	const auto abs_path = file.absoluteFilePath();

#ifdef Q_OS_WIN
	if (abs_path.length() >= 260) { // video files cannot be played on windows if file path is too long
		QMessageBox::critical(this,
			tr("Error opening media"),
			tr("<p>Could not open <b>%1</b></p>"
			   "<p>File path is too long: %2 characters, max 259.</p>")
				.arg(file.fileName())
				.arg(abs_path.length()));
		return false;
	}
#endif
	stopVideo();
	bool res = m_playlist.addMedia(QUrl::fromLocalFile(abs_path));
	if (!res) {
		QMessageBox::critical(this,
			tr("Error opening media"),
			tr("<p>Could not open <b>%1</b></p>"
			   "<p>Could not add media to playlist.</p>")
				.arg(file.fileName()));
		return false;
	}

	m_picture.hide();
	m_video.show();
	playMedia();
	return true;
}

void Tagger::hideVideo()
{
	stopVideo();
	m_video.hide();
	m_picture.show();
}

void Tagger::stopVideo()
{
	m_player.stop();
	m_playlist.clear();
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
	emit fileOpened(currentFile());
	findTagsFiles();

	if(m_file_queue.size() < 2)
		return true;

	QSettings s;
	if(!s.value(QStringLiteral("performance/pixmap_precache_enabled"), true).toBool())
		return true;

	int preloadcount = abs(s.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt());
	preloadcount = std::max(std::min(preloadcount, 16), 1);
	for(int i = -preloadcount; i <= preloadcount; ++i)
	{
		if(i == 0)
			continue;
		auto idx = ptrdiff_t(m_file_queue.currentIndex())+i;

		auto filepath = m_file_queue.nth(idx);
		if (QDir::match(util::supported_image_formats_namefilter(),
		                QFileInfo(filepath).fileName())) {

			m_picture.cache.addFile(filepath, m_picture.size(), m_picture.devicePixelRatioF());
		}
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

#ifdef Q_OS_WIN
	if (mediaIsVideo()) { // video files cannot be played on windows if file path is too long
		if (new_file_path.length() >= 260) {
			QMessageBox::critical(this,
				tr("Error opening media"),
				tr("<p>Could not open <b>%1</b></p>"
				   "<p>File path is too long: %2 characters, max 259.</p>")
					.arg(filename)
					.arg(new_file_path.length()));
			return RenameStatus::Cancelled;
		}
	}
#endif

	if(new_file_path == m_file_queue.current() || m_input.text().isEmpty())
		return RenameStatus::Failed;


	int messagebox_reply = 0;
	bool force_rename = options.testFlag(RenameOption::ForceRename);
	if (!force_rename) {

		// these should already be sorted by fixTags() call above.
		// original tags list is sorted when initially created.
		auto current_tags = m_input.tags_list();

		QString added_tags, removed_tags;
		getTagDifference(m_original_tags, current_tags, added_tags, removed_tags, false);

		// only show removed tags diff when it wasn't just the hash filename
		if (m_original_tags.size() == 1 && util::is_hex_string(m_original_tags[0])) {
			removed_tags.clear();
		}

		/* Show save dialog */
		QMessageBox renameMessageBox(QMessageBox::Question,
			tr("Rename file?"),
			tr("<p>Rename <b>%1</b>?</p>").arg(file.completeBaseName()) + added_tags + removed_tags,
			QMessageBox::Save|QMessageBox::Discard);

		renameMessageBox.addButton(QMessageBox::Cancel);
		renameMessageBox.setButtonText(QMessageBox::Save, tr("Rename"));
		renameMessageBox.setButtonText(QMessageBox::Discard, tr("Discard"));
		messagebox_reply = renameMessageBox.exec();
	}

	QSettings settings;
	if(force_rename || messagebox_reply == QMessageBox::Save) {

		const bool media_is_video = mediaIsVideo();
		if(media_is_video) {
			// Unlike images, the whole video file is NOT loaded into memory, so we have to stop playback
			// to rename video file on windows.
			// TODO: don't stop playback on linux.
			stopVideo();
		}

		auto result = m_file_queue.renameCurrentFile(new_file_path);

		switch (result) {
		case FileQueue::RenameResult::SourceFileMissing:
		case FileQueue::RenameResult::GenericFailure:
			// see above comment; restart playback.
			if (media_is_video) {
				loadVideo(currentFile());
			}

			QMessageBox::warning(this,
				tr("Could not rename file"),
				tr("<p>Could not rename <b>%1</b></p>"
				   "<p>File may have been renamed or removed by another application, "
				   "file with this name may already exist in the current directory or exceed the character limit.</p>")
					.arg(file.fileName()));

			return RenameStatus::Failed;

		case FileQueue::RenameResult::TargetFileExists:
			// see above comment; restart playback.
			if (media_is_video) {
				loadVideo(currentFile());
			}

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
			// see above comment; restart playback if necessary.
			if (media_is_video && options.testFlag(RenameOption::ReopenFile)) {
				loadVideo(currentFile());
			}
			emit fileRenamed(m_input.text());
			return RenameStatus::Renamed;

		}
	}

	if(messagebox_reply == QMessageBox::Cancel) {
		return RenameStatus::Cancelled;
	}

	if(messagebox_reply == QMessageBox::Discard) {
		// NOTE: restore to initial state to prevent multiple rename dialogs
		m_input.setText(file.completeBaseName());
		return RenameStatus::NotModified;
	}

	return RenameStatus::Failed;
}
