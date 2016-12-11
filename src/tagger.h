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
#include <QStringList>
#include <QFileSystemWatcher>
#include <memory>

#include "util/unordered_map_qt.h"
#include "picture.h"
#include "input.h"
#include "file_queue.h"
#include "statistics.h"

/**
 * Main widget of application. Contains Picture viewer and TagInput.
 */
class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override = default;

	/// Opens file, session or directory.
	bool open(const QString& filename);

	/// Opens file and enqueues its directory.
	bool openFile(const QString&);

	/// Enqueues directory contents and opens first file.
	bool openDir(const QString&);

	/// Opens tagging session from file.
	bool openSession(const QString& sfile);

	/// Opens file with specified index in queue.
	bool openFileInQueue(size_t index = 0);

	/// Result of rename operation
	enum class RenameStatus
	{
		Cancelled = -1, ///< Rename was cancelled by user.
		Failed = 0,     ///< Rename failed.
		Renamed = 1     ///< Renamed successfully
	};
	Q_ENUM(RenameStatus)

	/// Options controlling UI behavior when renaming file.
	enum class RenameOption
	{
		NoOption = 0x0,          ///< Default UI behavior.
		ForceRename = 0x1,       ///< Force rename - do not show rename dialog.
		HideCancelButton = 0x2,  ///< Remove Cancel button.
	};
	using RenameOptions = QFlags<RenameOption>;
	Q_FLAG(RenameOptions)

	/**
	 * \brief Renames current file
	 * \param options UI behavior modifiers.
	 * \retval RenameStatus::Renamed Renamed successfully
	 * \retval RenameStatus::Cancelled Rename cancelled by user.
	 * \retval RenameStatus::Failed Failed to rename.
	 */
	RenameStatus rename(RenameOptions options = RenameOption::NoOption);


	/**
	 * \brief Asks to rename current file and opens next.
	 * \param options UI behavior modifiers.
	 */
	void nextFile(RenameOptions options = RenameOption::NoOption);


	/**
	 * \brief Asks to rename current file and opens previous.
	 * \param options UI behavior modifiers.
	 */
	void prevFile(RenameOptions options = RenameOption::NoOption);

	/// Returns whether current file name is modified by user.
	bool    fileModified()    const;

	/// Returns whether the file queue is empty.
	bool    isEmpty() const;

	/// Returns dimensions of current media.
	QSize   mediaDimensions() const;

	/// Returns file size of current media.
	size_t  mediaFileSize()     const;

	/// Returns imageboard post URL of current media.
	QString postURL()         const;

	/// Returns current media path.
	QString currentFile()     const;

	/// Returns path to directory of current media.
	QString currentDir()      const;

	/// Returns file name of current media.
	QString currentFileName() const;

	/// Returns file type of current media.
	QString currentFileType() const;

	/// Returns reference to FileQueue.
	FileQueue& queue();

	/// Returns reference to TaggerStatistics
	TaggerStatistics& statistics();

public slots:
	/// Sets Tag Input Visibility.
	void setInputVisible(bool visible);

	/// Asks to delete currently selected file.
	void deleteCurrentFile();

	/// Finds new set of tag files used for autocomplete.
	void reloadTags();

	/// Opens current tag files set in default editor.
	void openTagFilesInEditor();

	/// Opens current tag files set in default file browser.
	void openTagFilesInShell();

	/// Updates configuration from QSettings.
	void updateSettings();

signals:

	/**
	 * \brief File successfully opened.
	 * \retval File path.
	 */
	void fileOpened(const QString&);


	/**
	 * \brief File name modified by user.
	 * \retval New file name.
	 */
	void tagsEdited(const QString&);


	/**
	 * \brief File renamed by user.
	 * \retval New file name.
	 */
	void fileRenamed(const QString&);


	/**
	 * \brief New Tags were added by user.
	 * \retval List of tags not in tag file added for current file.
	 */
	void newTagsAdded(const QStringList&);


	/**
	 * @brief Tag file contents changed by user.
	 */
	void tagFileChanged();

private:
	void findTagsFiles(bool force = false);
	void reloadTagsContents();
	bool loadCurrentFile();
	bool loadFile(size_t index, bool silent = false);
	void updateNewTagsCounts();
	void clear();

	static constexpr int m_tag_input_layout_margin = 10;

	QVBoxLayout m_main_layout;
	QVBoxLayout m_tag_input_layout;

	QFrame      m_separator;
	Picture     m_picture;
	TagInput    m_input;
	TaggerStatistics m_statistics;

	FileQueue   m_file_queue;
	QString     m_previous_dir;
	QStringList m_current_tag_files;
	std::unordered_map<QString, unsigned> m_new_tag_counts;
	std::unique_ptr<QFileSystemWatcher> m_fs_watcher;
	unsigned    m_overall_new_tag_counts = 0u;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Tagger::RenameOptions)
#endif // WISETAGGER_H
