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

/*!
 * Main widget of application. Contains Picture viewer and TagInput.
 */
class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *_parent = nullptr);
	~Tagger() override;

	/*!
	 * \brief Opens file and enqueues its directory.
	 */
	void openFile(const QString&);


	/*!
	 * \brief Enqueues directory contents and opens first file.
	 */
	void openDir (const QString&);

	/*!
	 * \brief Renames current file
	 * \param flags UI behavior modifiers. See ::RenameFlags description.
	 * \retval RenameStatus::Renamed Renamed successfully
	 * \retval RenameStatus::Cancelled Rename cancelled by user.
	 * \retval RenameStatus::Failed Failed to rename.
	 */
	RenameStatus rename(RenameFlags flags  = RenameFlags::Default);


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


	/// Loads currently selected file.
	void loadCurrentFile();

	/// Asks to delete currently selected file.
	void deleteCurrentFile();

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

public slots:


	/// Reloads tag files used for autocomplete.
	void reloadTags();

signals:

	/// File successfully opened.
	void fileOpened(const QString&);

	/// File name modified by user.
	void tagsEdited(const QString&);

	/// Imageboad post url changed.
	void postURLChanged(const QString&);

private:
	bool loadFile(size_t index);
	void findTagsFiles();
	void clear();

	QVBoxLayout m_main_layout;
	QVBoxLayout m_tag_input_layout;

	QFrame      m_separator;
	Picture     m_picture;
	TagInput    m_input;

	FileQueue   m_file_queue;
	QString     m_previous_dir;
	QStringList m_current_tag_files;
};

#endif // WISETAGGER_H
