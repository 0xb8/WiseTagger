/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef WISETAGGER_H
#define WISETAGGER_H

/*! \file tagger.h
 *  \brief Class \ref Tagger
 */

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QFrame>
#include <QWidget>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QBasicTimer>
#include <memory>

#include <QtMultimediaWidgets/QVideoWidget>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QMediaPlaylist>

#include "util/unordered_map_qt.h"
#include "util/tag_fetcher.h"
#include "picture.h"
#include "input.h"
#include "file_queue.h"


/*!
 * \brief Main widget of the application.
 *
 * Contains Picture viewer and TagInput.
 */
class Tagger : public QWidget
{
	Q_OBJECT

public:

	/// Constructs the Tagger widget.
	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override = default;

	/// Open file, session or directory.
	bool open(const QString& filename);

	/// Open file and enqueue its directory.
	bool openFile(const QString&);

	/// Enqueue directory contents and open first file.
	bool openDir(const QString&);

	/// Open tagging session from file.
	bool openSession(const QString& sfile);

	/// Open tagging session from serialized data.
	bool openSession(const QByteArray& sdata);

	/// Open file with specified index in queue.
	bool openFileInQueue(size_t index = 0);

	/// Result of rename operation
	enum class RenameStatus
	{
		Cancelled   = -1, ///< Rename was cancelled by user.
		Failed      = 0,  ///< Rename failed.
		Renamed     = 1,  ///< Renamed successfully.
		NotModified = 2   ///< File name was not modified.
	};
	Q_ENUM(RenameStatus)

	/// Options controlling UI behavior when renaming file.
	enum class RenameOption
	{
		NoOption    = 0x0,  ///< Default UI behavior.
		ForceRename = 0x1,  ///< Force rename - do not show rename dialog.
		ReopenFile  = 0x2   ///< Reopen this file after renaming.
	};
	/// Bitflags for \ref RenameOption
	using RenameOptions = QFlags<RenameOption>;
	Q_FLAG(RenameOptions)

	/// Options controlling skipping files in queue when selecting next or previous file.
	enum class SkipOption
	{
		DontSkip      = 0x0,  ///< Default behavior, don't skip any files.
		SkipToFixable = 0x1,  ///< Skip until fixable file is found.
	};
	/// Bitflags for \ref SkipOption
	using SkipOptions = QFlags<SkipOption>;
	Q_FLAG(SkipOptions)


	/*!
	 * \brief Determine whether it is safe to close the application.
	 * \param status Rename operation status, see Tagger::rename().
	 */
	static bool canExit(RenameStatus status);

	/*!
	 * \brief Rename current file
	 * \param options UI behavior modifiers (see \ref RenameOption).
	 * \retval RenameStatus::Renamed Renamed successfully
	 * \retval RenameStatus::Cancelled Rename cancelled by user.
	 * \retval RenameStatus::Failed Failed to rename.
	 */
	RenameStatus rename(RenameOptions options = RenameOption::NoOption);

	/*!
	 * \brief Ask user to rename current file and opens next.
	 * \param options UI behavior modifiers (see \ref RenameOption).
	 * \param skip_option Skip files option (see \ref SkipOption)
	 */
	void nextFile(RenameOptions options = RenameOption::NoOption,
	              SkipOptions skip_option = SkipOption::DontSkip);


	/*!
	 * \brief Ask user to rename current file and opens previous.
	 * \param options UI behavior modifiers (see \ref RenameOption).
	 * \param skip_option Skip files option (see \ref SkipOption)
	 */
	void prevFile(RenameOptions options = RenameOption::NoOption,
	              SkipOptions skip_option = SkipOption::DontSkip);

	/*!
	 * \brief Set tag input text.
	 */
	void setText(const QString& text);

	/// Current tag input text.
	QString text() const;

	/// Undo tag input changes and set original tags.
	void    resetText();

	/// Is current file name modified by user.
	bool    fileModified() const;

	/// Is current file renameable.
	bool    fileRenameable() const;

	/// Is the file queue empty.
	bool    isEmpty() const;

	/// Are tag file(s) present.
	bool    hasTagFile() const;

	/// Is current media a video.
	bool    mediaIsVideo() const;

	/// Is current media an animated image, eg. a GIF.
	bool    mediaIsAnimatedImage() const;

	/// Is current media playing.
	bool    mediaIsPlaying() const;

	/// Dimensions of current media.
	QSize   mediaDimensions() const;

	/// Framerate of current media. 0 if not a video.
	float   mediaFramerate() const;

	/// File size of current media.
	size_t  mediaFileSize()     const;

	/// Imageboard post URL of current media.
	QString postURL()         const;

	/// Current media path.
	QString currentFile()     const;

	/// Path to directory of current media.
	QString currentDir()      const;

	/// File name of current media.
	QString currentFileName() const;

	/// File type of current media.
	QString currentFileType() const;

	/// Last modified date of current media.
	QDateTime currentFileLastModified() const;

	/// Filter string that filename must match to be selected in queue.
	QString queueFilter() const;

	/// Returns current edit mode.
	EditMode editMode() const noexcept;

	/// Returns pointer to current tags completion model.
	QAbstractItemModel* completionModel();

	/// Reference to FileQueue.
	FileQueue& queue();

	/// Reference to TagFetcner.
	TagFetcher& tag_fetcher();

public slots:
	/// Set Tag Input visibility.
	void setInputVisible(bool visible);

	/// Set Tag input view mode.
	void setViewMode(ViewMode view_mode);

	/// Ask user to delete currently selected file.
	void deleteCurrentFile();

	/// Applies tag transformations to current file name.
	void fixTags();

	/// Fetches tags from associated imageboard.
	void fetchTags();

	/// Find new set of tag files used for autocomplete.
	void reloadTags();

	/// Open current tag files set in default editor.
	void openTagFilesInEditor();

	/// Open dialog to edit temporaty tag files.
	void openTempTagFileEditor();

	/// Open current tag files set in default file browser.
	void openTagFilesInShell();

	/// Update configuration from QSettings.
	void updateSettings();

	/// Pause media playback.
	void pauseMedia();

	/// Resume media playback.
	void playMedia();

	/// Set media playback state (playing/paused).
	void setMediaPlaying(bool playing);

	/// Set media mute state.
	void setMediaMuted(bool muted);

	/// Set filter string that filename must match to be selected in queue.
	void setQueueFilter(QString filter_str);

	/// Display status information on the bottom left and right
	void setStatusText(QString left, QString right);

	/// Set current edit mode
	void setEditMode(EditMode mode);

signals:
	/// Emitted when media file has been successfully opened.
	void fileOpened(const QString& file);

	/// Emitted when session file has been sucessfully opened.
	void sessionOpened(const QString& sfile);

	/// Emitted when \ref queue() becomes empty.
	void cleared();

	/*!
	 * \brief Emitted when file name has been modified by user.
	 * \param newname New file name.
	 */
	void tagsEdited(const QString& newname);

	/*!
	 * \brief Emitted when file has been renamed by user.
	 * \param newname New file name.
	 */
	void fileRenamed(const QString& newname);

	/*!
	 * \brief Emitted when new tags have been added by user.
	 * \param newtags List of tags not in tag file that were added for current file.
	 */
	void newTagsAdded(const QStringList& newtags);

	/// Emitted when tag file contents have been changed externally.
	void tagFileChanged();

	/*!
	 * \brief Emitted when no tag files were found.
	 * \param normal_file Normal tag file name.
	 * \param override_file Override tag file name.
	 * \param paths List of paths that have been searched for tag files.
	 */
	void tagFilesNotFound(QString normal_file, QString override_file, QStringList paths);

	/*!
	 * \brief Emitted when conflicting tag files were found.
	 * \param normal_file Normal tag file name.
	 * \param override_file Override tag file name.
	 * \param files List of conflicting tag files.
	 */
	void tagFilesConflict(QString normal_file, QString override_file, QStringList files);

	/*!
	 * \copydoc TagInput::parseError()
	 */
	void parseError(QString regex_source, QString error, int column);

	/*!
	 * \brief Emitted on double Escape combination to hide the window to tray.
	 */
	void hideRequested();

	/*!
	 * \brief Emitted when picture label link is clicked.
	 */
	void linkActivated(QString link);

protected:
	void keyPressEvent(QKeyEvent* e) override;
	void timerEvent(QTimerEvent* e) override;
	void focusInEvent(QFocusEvent* e) override;

private:
	void findTagsFiles(bool force = false);
	void reloadTagsContents();
	bool loadCurrentFile();
	static bool isFileRenameable(const QFileInfo& fi);
	bool selectWithFixableTags(int direction);
	bool loadFile(size_t index, bool silent = false);
	bool loadVideo(const QFileInfo& file);
	void hideVideo();
	void stopVideo();
	void updateNewTagsCounts();
	void clear();
	void tagsFetched(QString file, QString tags);
	void getTagDifference(QStringList current_tags, QStringList new_tags, QString& added, QString& removed, bool show_merge_hint);
	QByteArray read_tag_data(const QStringList& tags_files);

	static constexpr int m_tag_input_layout_margin = 10;

	QVBoxLayout m_main_layout;
	QVBoxLayout m_tag_input_layout;

	QFrame       m_separator;
	Picture      m_picture;
	QVideoWidget m_video;
	TagInput     m_input;
	TagFetcher   m_fetcher;
	QBasicTimer  m_hide_request_timer;

	FileQueue   m_file_queue;
	QMediaPlaylist m_playlist;
	QMediaPlayer m_player;

	QString     m_previous_dir;
	QStringList m_current_tag_files;
	QString     m_temp_tags;
	QStringList m_original_tags;

	QString     m_queue_filter_src;

	std::unordered_map<QString, unsigned> m_new_tag_counts;
	std::unique_ptr<QFileSystemWatcher> m_fs_watcher;
	unsigned    m_overall_new_tag_counts = 0u;

	bool        m_media_muted = false;
	bool        m_file_renameable = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Tagger::RenameOptions)
#endif // WISETAGGER_H
