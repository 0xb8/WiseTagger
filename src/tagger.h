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
#include <memory>

#include "util/unordered_map_qt.h"
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
		ForceRename = 0x1   ///< Force rename - do not show rename dialog.
	};
	using RenameOptions = QFlags<RenameOption>;
	Q_FLAG(RenameOptions)

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
	 */
	void nextFile(RenameOptions options = RenameOption::NoOption);


	/*!
	 * \brief Ask user to rename current file and opens previous.
	 * \param options UI behavior modifiers (see \ref RenameOption).
	 */
	void prevFile(RenameOptions options = RenameOption::NoOption);

	/// Is current file name modified by user.
	bool    fileModified()    const;

	/// Is the file queue empty.
	bool    isEmpty() const;

	/// Are tag file(s) present.
	bool    hasTagFile() const;

	/// Dimensions of current media.
	QSize   mediaDimensions() const;

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

	/// Reference to FileQueue.
	FileQueue& queue();

public slots:
	/// Set Tag Input visibility.
	void setInputVisible(bool visible);

	/// Ask user to delete currently selected file.
	void deleteCurrentFile();

	/// Applies tag transformations to current file name.
	void fixTags();

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

signals:
	/// Emitted when media file has been successfully opened.
	void fileOpened(const QString& file);

	/// Emitted when session file has been sucessfully opened.
	void sessionOpened(const QString& sfile);

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
	 * \copydoc TagInput::parseError()
	 */
	void parseError(QString regex_source, QString error, int column);

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

	FileQueue   m_file_queue;
	QString     m_previous_dir;
	QStringList m_current_tag_files;
	QString     m_temp_tags;
	std::unordered_map<QString, unsigned> m_new_tag_counts;
	std::unique_ptr<QFileSystemWatcher> m_fs_watcher;
	unsigned    m_overall_new_tag_counts = 0u;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Tagger::RenameOptions)
#endif // WISETAGGER_H
