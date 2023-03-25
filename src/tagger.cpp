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
#include "util/misc.h"
#include "util/size.h"
#include "util/strings.h"
#include "util/open_graphical_shell.h"
#include "util/tag_file.h"
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
	m_file_queue.setExtensionFilter(util::supported_image_formats_namefilter() +
	                                util::supported_video_formats_namefilter());

	m_main_layout.setContentsMargins(0, 0, 0, 0);
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
	connect(&m_picture, &Picture::linkActivated, this, &Tagger::linkActivated);
	connect(&m_picture, &Picture::mediaResized, this, &Tagger::mediaResized);
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
	m_nav_direction = 0;
	emit cleared();
}

bool Tagger::open(const QString& filename, bool recursive)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	bool res     = openDir(filename, recursive);
	if(!res) res = openSession(filename);
	if(!res) res = openFile(filename, recursive);
	return res;
}

bool Tagger::openFile(const QString &filename, bool recursive)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	const QFileInfo fi(filename);
	if(!fi.isReadable() || !fi.isFile()) {
		return false;
	}
	qApp->setOverrideCursor(Qt::WaitCursor);
	m_file_queue.clear();
	m_file_queue.push(fi.absolutePath(), recursive);
	m_file_queue.sort();
	m_file_queue.select(m_file_queue.find(fi.absoluteFilePath()));
	m_picture.cache.clear();
	m_nav_direction = 0;

	qApp->restoreOverrideCursor();
	return loadCurrentFile();
}

bool Tagger::openDir(const QString &dir, bool recursive)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	const QFileInfo fi(dir);
	if(!fi.isReadable() || !fi.isDir()) {
		return false;
	}
	qApp->setOverrideCursor(Qt::WaitCursor);
	m_file_queue.clear();
	m_file_queue.push(dir, recursive);
	m_file_queue.sort();
	m_file_queue.select(0u); // NOTE: must select after sorting, otherwise selects first file in directory order
	m_picture.cache.clear();
	m_nav_direction = 0;

	qApp->restoreOverrideCursor();
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
	m_nav_direction = 0;

	bool res = loadCurrentFile();
	if(res) {
		emit sessionOpened(sfile);
	}
	return res;
}

bool Tagger::openSession(const QByteArray & sdata)
{
	if(rename() == RenameStatus::Cancelled)
		return false;

	if(!m_file_queue.loadFromMemory(sdata)) {
		QMessageBox::critical(this,
			tr("Load Session Failed"),
			tr("<p>Could not load session from memory buffer</p>"));
		return false;
	}
	m_picture.cache.clear();
	m_nav_direction = 0;

	bool res = loadCurrentFile();
	return res;
}

void Tagger::nextFile(RenameOptions options, SkipOptions skip_option)
{
	if(rename(options) == RenameStatus::Cancelled)
		return;

	if (skip_option & SkipOption::SkipToFixable) {
		qApp->setOverrideCursor(Qt::WaitCursor);
		bool found = selectWithFixableTags(+1);
		qApp->restoreOverrideCursor();

		if (!found) {
			QMessageBox::information(this,
			                         tr("Could not find next fixable file"),
			                         tr("<p>Could not find next fixable file in queue.</p>"
			                            "<p>Perhaps all files in the queue are fixed?</p>"));
		}
	} else {
		m_file_queue.forward();
		m_nav_direction = +1;
	}

	loadCurrentFile();
}

void Tagger::prevFile(RenameOptions options, SkipOptions skip_option)
{
	if(rename(options) == RenameStatus::Cancelled)
		return;

	if (skip_option & SkipOption::SkipToFixable) {
		qApp->setOverrideCursor(Qt::WaitCursor);
		bool found = selectWithFixableTags(-1);
		qApp->restoreOverrideCursor();

		if (!found) {
			QMessageBox::information(this,
			                         tr("Could not find previous fixable file"),
			                         tr("<p>Could not find previous fixable file in queue.</p>"
			                            "<p>Perhaps all files in  the queue are fixed?</p>"));
		}
	} else {
		m_file_queue.backward();
		m_nav_direction = -1;
	}

	loadCurrentFile();
}

void Tagger::setText(const QString & text)
{
	int path_length_limit = 255 - currentFileSuffix().length() - 1;

#ifdef Q_OS_WIN32
	// While Qt on Windows supports 32K characters path length,
	// Windows explorer still has troubles with anything longer than 260 chars :(
	path_length_limit -= currentDir().length() + 1 - 4;
#endif

	m_input.setText(text, path_length_limit);
}

QString Tagger::text() const
{
	return m_input.text();
}

void Tagger::addTags(QString tags, int *tag_count)
{
	TagEditState state;
	auto options = TagParser::FixOptions::from_settings();
	options.sort = true;

	// Autofix imageboard tags before comparing and assigning
	util::replace_special(tags);
	auto fixed_tags_list = m_input.tag_parser().fixTags(state, tags, options);
	tags = util::join(fixed_tags_list);
	if (tag_count) {
		*tag_count = fixed_tags_list.size();
	}

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
			                    "<p>Please choose what to do:</p>").arg(tags)
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
			setText(tags);
		}
	}
}

void Tagger::resetText()
{
	m_last_caption_file_contents.clear();
	auto current_fi = QFileInfo(m_file_queue.current());

	switch (m_tag_storage) {
	case TagStorage::Filename:
		setText(current_fi.completeBaseName());
		break;
	case TagStorage::CaptionFile:
	        {
		        auto caption_data = readCaptionFile(current_fi);
			setText(caption_data.isEmpty() ? current_fi.completeBaseName() : caption_data);
			break;
	        }
	}
}

bool Tagger::openFileInQueue(size_t index)
{
	m_file_queue.select(index);
	m_nav_direction = 0;
	return loadCurrentFile();
}

void Tagger::deleteCurrentFile()
{
	const auto current_file = currentFile();
	auto last_modified = QFileInfo(currentFile()).lastModified();

	QMessageBox delete_msgbox(QMessageBox::Question, tr("Delete file?"),
		tr("<h3 style=\"font-weight: normal;\">Are you sure you want to delete <u>%1</u> permanently?</h3>"
		   "<dd><dl>File type: %2</dl>"
		   "<dl>File size: %3</dl>"
		   "<dl>Dimensions: %4 x %5</dl>"
		   "<dl>Modified: %6 (%7 ago)</dl></dd>"
		   "<p><em>This action cannot be undone!</em></p>").arg(
			currentFileName(),
			currentFileType(),
			util::size::printable(mediaFileSize()),
			QString::number(mediaDimensions().width()),
			QString::number(mediaDimensions().height()),
			last_modified.toString(tr("yyyy-MM-dd hh:mm:ss", "modified date")),
			util::duration(last_modified.secsTo(QDateTime::currentDateTime()), false)),
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
		m_picture.cache.invalidate(current_file);
		if (m_nav_direction < 0) {
			m_file_queue.backward();
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
		addTags(tags, &processed_tags_count);
	}

	TaggerStatistics::instance().tagsFetched(overall_tags_count, processed_tags_count);
}

void Tagger::getTagDifference(QStringList old_tags,
                              QStringList new_tags,
                              QString & added_tags_str,
                              QString & removed_tags_str,
                              bool show_merge_hint)
{
	// input lists may not be sorted
	std::sort(old_tags.begin(), old_tags.end());
	std::sort(new_tags.begin(), new_tags.end());

	QStringList tags_added;
	std::set_difference(new_tags.begin(), new_tags.end(),
	                    old_tags.begin(), old_tags.end(),
	                    std::back_inserter(tags_added));

	QStringList tags_removed;
	std::set_difference(old_tags.begin(), old_tags.end(),
	                    new_tags.begin(), new_tags.end(),
	                    std::back_inserter(tags_removed));

	// set of all known tags
	const auto all_tags_set = QSet<QString>::fromList(m_input.tag_parser().getAllTags());
	const auto& parser = m_input.tag_parser();

	// sort removed/added tags respecting tag weights
	parser.sortTags(tags_removed.begin(), tags_removed.end());
	parser.sortTags(tags_added.begin(), tags_added.end());

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
	std::transform(new_tags.begin(), new_tags.end(), new_tags.begin(), mark_existing_tags);
	std::transform(tags_added.begin(), tags_added.end(), tags_added.begin(), mark_existing_tags);
	std::transform(tags_removed.begin(), tags_removed.end(), tags_removed.begin(), mark_existing_tags);

	if (!tags_added.empty()) {
		added_tags_str = QStringLiteral("<p style=\"margin-top: 1.6em;\">");
		added_tags_str.append(tr("Tags that will be added:"));
		added_tags_str.append(QStringLiteral("<p style=\"margin-left: 1em; line-height: 130%;\">"
		                                     "<code style=\"color:green;\">%1</code></p></p>").arg(util::join(tags_added)));
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
		                                       "<code style=\"color:red;\">%2</code></p></p>").arg(util::join(tags_removed)));
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

QString Tagger::currentFileSuffix() const
{
	return QFileInfo(m_file_queue.current()).suffix();
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

QString Tagger::queueFilter() const
{
	return m_queue_filter_src;
}

EditMode Tagger::editMode() const noexcept
{
	return m_input.editMode();
}

QAbstractItemModel * Tagger::completionModel()
{
	return m_input.completionModel();
}

bool Tagger::fileModified() const
{
	if(isEmpty()) // NOTE: to avoid FileQueue::current() returning invalid reference.
		return false;

	auto current_fi = QFileInfo(m_file_queue.current());

	switch (m_tag_storage) {
	case TagStorage::Filename:
		return m_input.text() != current_fi.completeBaseName();
	case TagStorage::CaptionFile:
		return m_input.tags() != readCaptionFile(current_fi);
	}
	Q_UNREACHABLE();
}

bool Tagger::fileRenameable() const
{
	return m_file_renameable;
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

float Tagger::mediaZoomFactor() const
{
	if (isEmpty())
		return 0.0f;

	if (mediaIsVideo())
		return 1.0f;

	const QSizeF file_dimensions = mediaDimensions();
	const QSizeF display_dimensions = m_picture.mediaDisplaySize();

	return display_dimensions.width() / file_dimensions.width();
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

bool Tagger::upscalingEnabled() const
{
	return m_picture.upscalingEnabled();
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
	QSettings settings;
	auto mem_limit_kb = settings.value(QStringLiteral("performance/pixmap_cache_size"), 0ull).toULongLong() * 1024ull;
	if (mem_limit_kb) {
		m_picture.cache.setMemoryLimitKiB(mem_limit_kb);
	}

	auto thread_count = settings.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt() * 2;
	if (thread_count) {
		m_picture.cache.setMaxConcurrentTasks(thread_count);
	}
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

void Tagger::setQueueFilter(QString filter_str)
{
	auto filter = util::split_unquoted(filter_str);
	filter.removeDuplicates();
	m_queue_filter_src = util::join(filter);
	m_file_queue.setSubstringFilter(filter);
}

void Tagger::setStatusText(QString left, QString right)
{
	m_picture.setStatusText(left, right);
}

void Tagger::setEditMode(EditMode mode)
{
	m_input.setEditMode(mode);
}

void Tagger::setUpscalingEnabled(bool enabled)
{
	m_picture.setUpscalingEnabled(enabled);
}

void Tagger::rotateImage(bool clockwise)
{
	if (mediaIsVideo() || mediaIsAnimatedImage()) {
		pwarn << "Cannot rotate video / gif";
		return;
	}

	m_picture.setRotation(m_picture.rotation() + (clockwise ? 1 : -1));
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

	QSettings settings;
	auto view_mode = settings.value(QStringLiteral("window/view-mode")).value<ViewMode>();
	if(view_mode == ViewMode::Minimal)
		return; // NOTE: no need to update following properties because they've been taken care of already

	m_separator.setVisible(visible);
	if(visible) {
		m_tag_input_layout.setContentsMargins(m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin);
	} else {
		m_tag_input_layout.setContentsMargins(0, 0, 0, 0);
	}
}

void Tagger::setViewMode(ViewMode view_mode)
{
	if(view_mode == ViewMode::Minimal) {
		m_tag_input_layout.setContentsMargins(0, 0, 0, 0);
		m_separator.hide();
	}
	if(view_mode == ViewMode::Normal) {
		m_tag_input_layout.setContentsMargins(m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin,
		                                      m_tag_input_layout_margin);
		m_separator.show();
	}
	m_input.setViewMode(view_mode);
}

//------------------------------------------------------------------------------

void Tagger::reloadTags()
{
	findTagsFiles(true);
}

QByteArray Tagger::read_tag_data(const QStringList& tags_files) {
	QByteArray data;
	for(const auto& filename : qAsConst(tags_files)) {
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
	return data;
}

void Tagger::reloadTagsContents()
{
	auto data = read_tag_data(m_current_tag_files);
	m_input.loadTagData(data);
	m_input.clearTagEditState();
}

void Tagger::openTagFilesInEditor()
{
	bool ok = false;

	auto selections = m_current_tag_files;
	selections.prepend(tr("[ edit all tag files ]"));

	auto file = QInputDialog::getItem(this,
	                                  tr("Edit Tag File"),
	                                  tr("<p>Select tag file to edit:</p>"),
	                                  selections,
	                                  0,
	                                  false,
	                                  &ok);

	if (ok) {
		if (file != selections[0]) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(file));
		} else {
			for(const auto& file : qAsConst(m_current_tag_files))
				QDesktopServices::openUrl(QUrl::fromLocalFile(file));
		}
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
	util::open_files_in_gui_shell(m_current_tag_files);
}

static QString normal_tag_file_pattern()
{
	const QSettings settings;
	return settings.value(QStringLiteral("tags/normal"), QStringLiteral("*.tags.txt")).toString();
}

static QString override_tag_file_pattern()
{
	const QSettings settings;
	return settings.value(QStringLiteral("tags/override"), QStringLiteral("*.tags!.txt")).toString();
}

void Tagger::findTagsFiles(bool force)
{
	if(isEmpty())
		return;

	const auto current_dir = currentDir();
	if(current_dir == m_previous_dir && !force)
		return;

	if (!fileRenameable())
		return;

	m_previous_dir = current_dir;
	m_tag_storage = QFile::exists(current_dir + "/.caption_mode.txt") ? TagStorage::CaptionFile : TagStorage::Filename;

	const QString tagsfile = normal_tag_file_pattern();
	const QString override = override_tag_file_pattern();

	std::vector<QDir> search_dirs;
	{
		QStringList tag_files, conflicting_files;
		util::find_tag_files_in_dir(current_dir, tagsfile, override, search_dirs, tag_files, conflicting_files);

		if (!tag_files.isEmpty() && tag_files == m_current_tag_files && !force) {
			pdbg << "same tag files, skipping";
			return;
		}

		if (!conflicting_files.isEmpty()) {
			emit tagFilesConflict(tagsfile, override, conflicting_files);
		}

		m_current_tag_files = std::move(tag_files);
	}

	m_input.clearTagData();
	m_input.clearTagEditState();

	if(Q_LIKELY(!m_current_tag_files.isEmpty())) {
		pdbg << "found tag files:" << m_current_tag_files;

		m_fs_watcher = std::make_unique<QFileSystemWatcher>(nullptr);


		auto tag_files_to_watch = m_current_tag_files;

		for (const auto& file : qAsConst(m_current_tag_files)) {
			QFileInfo fi;
			fi.setFile(file);

			if (fi.isSymbolicLink()) {
				QStringList sequence;
				auto resolved = util::resolve_symlink_to_file(fi.filePath(), 30, &sequence);
				if (resolved.isEmpty()) {
					pwarn << "Could not resolve symbolic link:" << fi.filePath();
				} else {
					// add intermediate links to watch set too (skip first, it's already in the list)
					for (int i = 1; i < sequence.size(); ++i)
						tag_files_to_watch.append(sequence[i]);

					// add resolved tag file
					tag_files_to_watch.append(resolved);
				}
			}
		}

		pdbg << "watch set:" << tag_files_to_watch;

		m_fs_watcher->addPaths(tag_files_to_watch);
		connect(m_fs_watcher.get(), &QFileSystemWatcher::fileChanged, this, [this](const auto& f)
		{
			this->reloadTags();
			pdbg << "reloaded tags due to changed file:" << f;
			emit this->tagFileChanged();
		});

		// pass tag files to tag input
		reloadTagsContents();
	} else {
		QStringList search_paths_list;
		for (const auto& dir : qAsConst(search_dirs)) {
			search_paths_list.append(dir.path());
		}
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
		m_picture.setRotation(0);
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
	m_file_renameable = isFileRenameable(f);

	findTagsFiles(); // must call before setting input text for tag classification to work

	m_input.setToolTip(fileRenameable() ? QString() : tr("Renaming disabled: User has no write permission in this directory."));
	m_input.setEnabled(fileRenameable());
	resetText();

	// remember original tags so we can show the difference when renaming
	m_original_tags = m_input.tags_list();
	m_original_tags.sort();
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
		m_picture.cache.invalidate(m_file_queue.current());
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

	QSettings settings;
	if(!settings.value(QStringLiteral("performance/pixmap_precache_enabled"), true).toBool())
		return true;

	auto try_cache_file = [this](const auto& filepath) {
		if (QDir::match(util::supported_image_formats_namefilter(),
		                QFileInfo(filepath).fileName())) {

			m_picture.cache.addFile(filepath, m_picture.size(), m_picture.devicePixelRatioF());
		}
	};

	int preloadcount = abs(settings.value(QStringLiteral("performance/pixmap_precache_count"), 1).toInt());
	preloadcount = std::max(std::min(preloadcount, 16), 1);

	size_t index = m_file_queue.currentIndex();
	for (int i = 0; i < preloadcount; ++i) {
		const auto& filepath = m_file_queue.next(index);
		try_cache_file(filepath);
	}

	index = m_file_queue.currentIndex();
	for(int i = 0; i < preloadcount; ++i) {
		const auto& filepath = m_file_queue.prev(index);
		try_cache_file(filepath);
	}

	return true;
}

bool Tagger::isFileRenameable(const QFileInfo & fi)
{
	bool res = false;
	QFileInfo dir_fi(fi.absolutePath());
	if (dir_fi.exists()) {
		++qt_ntfs_permission_lookup;
		res = dir_fi.isWritable();
		--qt_ntfs_permission_lookup;
	}
	return res;
}

bool Tagger::selectWithFixableTags(int direction)
{
	Q_ASSERT(!m_file_queue.empty()); // infinite loop otherwise :)

	TagParser parser;
	QDir prev_dir;
	QStringList prev_tags_files;
	std::vector<QDir> search_dirs;

	const QString tagsfile = normal_tag_file_pattern();
	const QString override = override_tag_file_pattern();

	auto check_fixable = [&](const auto& file_path)
	{
		const QFileInfo file(file_path);

		const auto current_dir = file.absoluteDir();
		if (current_dir != prev_dir || prev_dir.isRelative()) {
			prev_dir = current_dir;
			search_dirs.clear();

			QStringList tags_files, ignored_tags;
			util::find_tag_files_in_dir(current_dir, tagsfile, override, search_dirs, tags_files, ignored_tags);

			if (tags_files != prev_tags_files) {
				pdbg << "found tag files:" << tags_files;
				auto tag_data = this->read_tag_data(tags_files);
				parser.loadTagData(tag_data);
				prev_tags_files = std::move(tags_files);
			}
		}

		auto orig_tags = file.completeBaseName();
		if (util::is_hex_string(orig_tags))
			return true;

		auto orig_tags_list = util::split(orig_tags);

		// check if all tags are unknown, or if some tags are removed/replaced
		bool all_unknown = true;
		bool any_removed = false;
		for (const auto& tag : qAsConst(orig_tags_list)) {
			auto kind = parser.classify(tag, orig_tags_list);
			if (!(kind & TagParser::TagKind::Unknown))
				all_unknown = false;
			if (kind & TagParser::TagKind::Removed)
				any_removed = true;
			if (kind & TagParser::TagKind::Replaced)
				any_removed = true;
		}

		if (all_unknown || any_removed)
			return true;

		// full tag fixing fallback (checks for sorting too)
		TagEditState state;
		auto options = TagParser::FixOptions::from_settings();
		options.sort = true;

		// Autofix imageboard tags before comparing
		util::replace_special(orig_tags);
		auto fixed_tags_list = parser.fixTags(state, orig_tags, options);
		auto new_tags = util::join(fixed_tags_list);

		return orig_tags != new_tags;
	};

	size_t first_match_index = m_file_queue.npos;
	size_t file_index = m_file_queue.currentIndex();
	while (true) {
		// check next or previous file in queue
		const auto& file_path = direction > 0 ? m_file_queue.next(file_index) : m_file_queue.prev(file_index);
		Q_ASSERT(!file_path.isEmpty());

		if (Q_UNLIKELY(first_match_index == m_file_queue.npos)) {
			// remember index of the first file matching filter
			first_match_index = file_index;
		} else if (Q_UNLIKELY(file_index == first_match_index)) {
			// wrapped around the entire set of files matching filter, nothing more to check...
			break;
		}

		if (check_fixable(file_path)) {
			openFileInQueue(file_index);
			m_nav_direction = direction;
			return true;
		}
	}

	// no fixable files found
	return false;
}

Tagger::RenameStatus Tagger::rename(RenameOptions options)
{
	if(!fileModified())
		return RenameStatus::NotModified;

	if (m_tag_storage == TagStorage::CaptionFile) {
		return updateCaption(options);
	}

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
		return c.unicode() < 31;
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
		resetText();
		return RenameStatus::NotModified;
	}

	return RenameStatus::Failed;
}

Tagger::RenameStatus Tagger::updateCaption(RenameOptions options)
{
	Q_ASSERT(m_tag_storage == TagStorage::CaptionFile);

	if (m_input.text().isEmpty())
		return RenameStatus::Failed;

	const QFileInfo file(m_file_queue.current());

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
		        tr("Update captions?"),
		        tr("<p>Update captions for <b>%1</b>?</p>").arg(file.completeBaseName()) + added_tags + removed_tags,
		        QMessageBox::Save|QMessageBox::Discard);

		renameMessageBox.addButton(QMessageBox::Cancel);
		renameMessageBox.setButtonText(QMessageBox::Save, tr("Update"));
		renameMessageBox.setButtonText(QMessageBox::Discard, tr("Discard"));
		messagebox_reply = renameMessageBox.exec();
	}

	QSettings settings;
	if(force_rename || messagebox_reply == QMessageBox::Save) {
		bool result = writeCaptionFile(file, m_input.tags_list());
		if (result) {
			return RenameStatus::Renamed;
		} else {
			QMessageBox::critical(this,
			        tr("Cannot update captions file"),
			        tr("<p>Cannot update captions for <b>%1</b></p>"
			           "<p>Error opening captions file for writing.</p>"
			           "<p>Please check the permissions and try again.</p>")
			                .arg(file.fileName()));

			return RenameStatus::Failed;
		}
	}

	if(messagebox_reply == QMessageBox::Cancel) {
		return RenameStatus::Cancelled;
	}

	if(messagebox_reply == QMessageBox::Discard) {
		// NOTE: restore to initial state to prevent multiple rename dialogs
		resetText();
		return RenameStatus::NotModified;
	}

	return RenameStatus::Failed;
}

QString Tagger::readCaptionFile(QFileInfo source_file) const
{
	if (!m_last_caption_file_contents.isEmpty())
		return m_last_caption_file_contents;

	auto caption_file_path = source_file.path() + "/" + source_file.completeBaseName() + ".txt";
	pdbg << "reading caption file" << caption_file_path;

	QFile caption_file{caption_file_path};
	if (!caption_file.open(QIODevice::ReadOnly|QIODevice::Text)) {
		return QString{};
	}

	QTextStream stream(&caption_file);
	stream.setCodec("UTF-8");

	auto captions = stream.readAll().remove(QChar(',')).trimmed();
	m_last_caption_file_contents = captions;
	return captions;
}

bool Tagger::writeCaptionFile(QFileInfo source_file, const QStringList& tags) const
{
	auto caption_file_path = source_file.path() +  "/" + source_file.completeBaseName() + ".txt";
	pdbg << "writing caption file" << caption_file_path;

	QFile caption_file{caption_file_path};
	if (!caption_file.open(QIODevice::WriteOnly|QIODevice::Text)) {
		return false;
	}

	m_last_caption_file_contents.clear();

	QTextStream stream(&caption_file);
	stream.setCodec("UTF-8");

	int counter = tags.size();
	for (const auto& tag : tags) {
		stream << tag;
		if (--counter) {
			stream << QStringLiteral(", ");
		}
	}
	return true;
}
