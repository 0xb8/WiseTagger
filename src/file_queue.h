/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef FILE_QUEUE_H
#define FILE_QUEUE_H

#include <QStringList>
#include <QFileInfo>
#include <QObject>
#include <deque>
#include "global_enums.h"

/*!
 * File Queue class allows to select certain file as current and perform typical
 * operations on it, such as removing or deleting that file.
 *
 * Stored file paths are absolute and all member functions taking file paths as
 * parameters expect them to be absolute too.
 */
class FileQueue {
public:

	/// Result of rename operation
	enum class RenameResult
	{
		GenericFailure    =-1,
		Success           = 0,
		SourceFileMissing = 1,
		TargetFileExists  = 2
	};

	/// Value used as invalid index.
	static constexpr size_t npos = std::numeric_limits<size_t>::max();

	/// Checks suffix of session files.
	static bool checkSessionFileSuffix(const QFileInfo&);

	/// Name Filter for session files.
	static const QString sessionNameFilter;

	/// Set sorting criteria to be used by default.
	void setSortBy(SortQueueBy criteria) noexcept;

	/*!
	 * \brief Sets file extensions filter used by FileQueue::push
	 *        and FileQueue::checkExtension
	 * \param filters List of extension wildcards, e.g. \a *.jpg
	 */
	void setNameFilter(const QStringList& filters) noexcept(false);


	/*!
	 * \brief Checks if file's suffix is allowed by currently set name filter.
	 * \param fi QFileInfo object to check
	 */
	bool checkExtension(const QFileInfo& fi) const noexcept;


	/*!
	 * \brief Enqueues a file or all files inside a directory.
	 * \param path Path to file or directory.
	 *
	 * If \p path is a file, enqueues it. If \p path is a directory,
	 * enqueues files inside \p path, according to to filters set by
	 * FileQueue::setNameFilter
	 *
	 * This function provides basic exception safety guarantee.
	 */
	void push(const QString& path);



	/*!
	 * \brief Assigns list of files to internal storage.
	 *
	 * This function provides strong exception safety guarantee.
	 */
	void assign(const QStringList& paths);


	/*!
	 * \brief Sorts queue taking natural numbering into account.
	 *
	 * Numbers in file names are compared by their value. This makes sorted
	 * file sequence feel more natural, as apposed to \a "1, 10, 11, 2, 20, 21, ..."
	 */
	void sort()  noexcept;


	/*!
	 * \brief Clears queue contents.
	 */
	void clear() noexcept;


	/*!
	 * \brief Selects next file in queue, possibly wrapping around.
	 * \return Next file.
	 * \retval EmptyString - Queue is empty.
	 */
	const QString& forward() noexcept;


	/*!
	 * \brief Selects previous file in queue, possibly wrapping around.
	 * \return Previous file.
	 * \retval EmptyString - Queue is empty.
	 */
	const QString& backward() noexcept;


	/*!
	 * \brief Returns Nth file in queue, wrapping around on negative indices.
	 * \retval EmptyString - Queue is empty.
	 *
	 * When \p index is negative, returns Nth file from the back of queue.
	 * When absolute value of \p index is larger than size of queue, the
	 * resulting file index is determined as remainder of \a index / queue size.
	 */
	const QString& nth(ssize_t index) noexcept;


	/*!
	 * \brief Selects file.
	 * \param index Index to select.
	 * \return Selected file.
	 * \retval EmptyString - Index is out of bounds or queue is empty.
	 */
	const QString& select(size_t index) noexcept;


	/*!
	 * \brief Returns current file.
	 * \return Current file.
	 * \retval EmptyString - Queue is empty.
	 */
	const QString& current() const noexcept;


	/*!
	 * \brief Checks if the queue is empty.
	 * \retval TRUE - Queue is empty.
	 * \retval FALSE - Queue is not expty.
	 */
	bool   empty()        const noexcept;


	/*!
	 * \return Number of files in queue.
	 */
	size_t size()         const noexcept;


	/*!
	 * \brief Returns index of currently selected file.
	 * \return Index of currently selected file.
	 * \retval FileQueue::npos - Queue is empty
	 */
	size_t currentIndex() const noexcept;


	/*!
	 * \brief Finds a file in queue.
	 * \param path Path to file.
	 *
	 * \return Index of \p path in queue.
	 * \retval FileQueue::npos - \p path was not found.
	 */
	size_t find(const QString& path) noexcept;


	/*!
	 * \brief Renames currently selected file.
	 * \param New file name.
	 *
	 * \returns Result of rename operation.
	 */
	RenameResult renameCurrentFile(const QString&);


	/*!
	 * \brief Deletes currently selected file permanently.
	 *
	 * \retval true  - Deleted successfully.
	 * \retval false - Could not delete file. Queue not modified.
	 */
	bool deleteCurrentFile();


	/*!
	 * \brief Erases currently selected file from queue.
	 */
	void eraseCurrent();


	/*!
	 * \brief Serializes file names in queue to file.
	 * \return Number of bytes written.
	 */
	size_t saveToFile(const QString& path) const;


	/*!
	 * \brief Loads file names into queue from specified file.
	 *        Previous contents of queue is lost.
	 * \return Number of entries added to queue.
	 */
	size_t loadFromFile(const QString& path);

private:
	static const QString m_empty;
	std::deque<QString>  m_files;
	QStringList          m_name_filters;
	size_t               m_current = npos;
	SortQueueBy          m_sort_by = SortQueueBy::FileName;
};

#endif
