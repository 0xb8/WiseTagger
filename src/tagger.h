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
#include "tagger_enums.h"
#include "file_queue.h"
#include "statistics.h"

/*!
 * Main widget of application. Contains Picture viewer and TagInput.
 */
class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override;

	/// Opens file and enqueues its directory.
	void openFile(const QString&);

	/// brief Enqueues directory contents and opens first file.
	void openDir(const QString&);

	/// Opens file with specified index in queue.
	void openFileInQueue(size_t index = 0);

	/// Opens tagging session from file.
	void openSession(const QString& sfile);

	/*!
	 * \brief Renames current file
	 * \param flags UI behavior modifiers. See ::RenameFlags description.
	 * \retval RenameStatus::Renamed Renamed successfully
	 * \retval RenameStatus::Cancelled Rename cancelled by user.
	 * \retval RenameStatus::Failed Failed to rename.
	 */
	RenameStatus rename(RenameFlags flags = RenameFlags::Default);


	/*!
	 * \brief Asks to rename current file and opens next.
	 * \param flags UI behavior modifiers. See ::RenameFlags description.
	 */
	void nextFile(RenameFlags flags = RenameFlags::Default);


	/*!
	 * \brief Asks to rename current file and opens previous.
	 * \param flags UI behavior modifiers. See ::RenameFlags description.
	 */
	void prevFile(RenameFlags flags = RenameFlags::Default);

	/// Returns whether current file name is modified by user.
	bool    fileModified()    const;

	/// Returns dimensions of current image.
	QSize   pictureDimensions() const;

	/// Returns file size of current image.
	qint64  pictureSize()     const;

	/// Returns imageboard post URL of current image.
	QString postURL()         const;

	/// Returns current image path.
	QString currentFile()     const;

	/// Returns path to directory of current image.
	QString currentDir()      const;

	/// Returns file name of current image.
	QString currentFileName() const;

	/// Returns file type of current image.
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

	/// Reloads tag files used for autocomplete.
	void reloadTags();

signals:

	/*!
	 * \brief File successfully opened.
	 * \retval File path.
	 */
	void fileOpened(const QString&);


	/*!
	 * \brief File name modified by user.
	 * \retval New file name.
	 */
	void tagsEdited(const QString&);


	/*!
	 * \brief File renamed by user.
	 * \retval New file name.
	 */
	void fileRenamed(const QString&);


	/*!
	 * \brief Imageboard post URL changed
	 * \retval New post URL.
	 */
	void postURLChanged(const QString&);


	/*!
	 * \brief New Tags were added by user.
	 * \retval List of tags not in tag file added for current file.
	 */
	void newTagsAdded(const QStringList&);

private:
	void loadCurrentFile();
	bool loadFile(size_t index);
	void findTagsFiles();
	void updateNewTagsCounts();
	void clear();

	QVBoxLayout m_main_layout;
	QVBoxLayout m_tag_input_layout;

	QFrame      m_separator;
	Picture     m_picture;
	TagInput    m_input;

	FileQueue   m_file_queue;
	QString     m_previous_dir;
	QStringList m_current_tag_files;
	std::unordered_map<QString, unsigned>   m_new_tag_counts;
	TaggerStatistics m_statistics;
};

#endif // WISETAGGER_H
