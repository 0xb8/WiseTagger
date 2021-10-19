/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef FILE_QUEUE_H
#define FILE_QUEUE_H

/** @file file_queue.h
 *  @brief Class @ref FileQueue
 */

#include <QStringList>
#include <QFileInfo>
#include <QObject>
#include <deque>
#include "global_enums.h"

/*!
 * \brief File Queue class designed for image viewing and renaming.
 *
 * Class \ref FileQueue allows to select certain file as current and perform typical
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

	/// Checks if suffix of session files is valid.
	static bool checkSessionFileSuffix(const QFileInfo&);

	/// Name Filter for session files.
	static const QString sessionNameFilter;

	/// Set sorting criteria to be used by default.
	void setSortBy(SortQueueBy criteria) noexcept;

	/*!
	 * \brief Set file extensions filter used by FileQueue::push
	 *        and FileQueue::checkExtension
	 * \param filters List of extension wildcards, \em e.g. \code *.jpg \endcode
	 */
	void setNameFilter(const QStringList& filters) noexcept(false);


	/*!
	 * \brief Check if file's suffix is allowed by currently set name filter.
	 * \param fileinfo File info object corresponding to file.
	 */
	bool checkExtension(const QFileInfo& fileinfo) const noexcept;


	/*!
	 * \brief Set filename substring filter.
	 * \param filters List of substring filters.
	 */
	void setSubstringFilter(const QStringList& filters);


	/*!
	 * \brief Is substring filter currently activated.
	 */
	bool substringFilterActive() const;


	/*!
	 * \brief Enqueue a file or all files inside a directory.
	 * \param path Path to file or directory.
	 *
	 * If \p path is a file, it is added to queue. If \p path is a directory,
	 * all files inside \p path are added to queue, according to filters set
	 * by \ref FileQueue::setNameFilter()
	 *
	 * \note This function provides basic exception safety guarantee.
	 */
	void push(const QString& path);



	/*!
	 * \brief Replace queue contents with list of \p paths.
	 *
	 * \note This function provides strong exception safety guarantee.
	 */
	void assign(const QStringList& paths);


	/*!
	 * \brief Sort queue taking natural numbering into account.
	 *
	 * Numbers in file names are compared by their value. This makes sorted
	 * file sequence feel more natural, as apposed to \a "1, 10, 11, 2, 20, 21, ..."
	 */
	void sort()  noexcept;


	/*!
	 * \brief Clear queue contents.
	 */
	void clear() noexcept;


	/*!
	 * \brief Select next file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 *
	 * \return Next file.
	 * \retval EmptyString Queue is empty.
	 */
	const QString& forward() noexcept;


	/*!
	 * \brief Return next file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 *
	 * \param[inout] from Starting index, on success set to the index of next file.
	 * \return Next file.
	 * \retval EmptyString Queue is empty. \p from is not modified.
	 */
	const QString& next(size_t& from) const noexcept;


	/*!
	 * \brief Select previous file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 *
	 * \return Previous file.
	 * \retval EmptyString Queue is empty.
	 */
	const QString& backward() noexcept;


	/*!
	 * \brief Return previous file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 *
	 * \param[inout] from Starting index, on success set to the index of previous file.
	 * \return Previous file.
	 * \retval EmptyString Queue is empty. \p from is not modified.
	 */
	const QString& prev(size_t& from) const noexcept;


	/*!
	 * \brief Return Nth file in queue, wrapping around on negative indices.
	 * \retval EmptyString Queue is empty.
	 *
	 * When \p index is negative, returns Nth file from the back of queue.
	 * When absolute value of \p index is larger than size of queue, the
	 * resulting file index is determined as follows:
	 * \code index % queue_size \endcode.
	 */
	const QString& nth(ptrdiff_t index) noexcept;


	/*!
	 * \brief Select file by index.
	 * \param index Index to select.
	 * \return Selected file.
	 * \retval EmptyString Index is out of bounds or queue is empty.
	 */
	const QString& select(size_t index) noexcept;


	/*!
	 * \brief Current file.
	 * \return Current file.
	 * \retval EmptyString Queue is empty.
	 */
	const QString& current() const noexcept;


	/*!
	 * \brief Does the current file match the current filter.
	 */
	bool currentFileMatchesQueueFilter() const noexcept;


	/*!
	 * \brief Does the \p file match the current filter.
	 */
	bool fileMatchesFilter(const QFileInfo& file) const;


	/*!
	 * \brief Check if the queue is empty.
	 */
	bool   empty()        const noexcept;


	/*!
	 * \brief Check if filtered queue is empty.
	 * \return
	 */
	bool   filteredEmpty() const noexcept;


	/*!
	 * \brief Number of files in queue.
	 */
	size_t size()         const noexcept;


	/*!
	 * \brief Number of files in queue that pass the filter.
	 */
	size_t filteredSize() const noexcept;


	/*!
	 * \brief Index of currently selected file.
	 * \return Index of currently selected file.
	 * \retval FileQueue::npos Queue is empty
	 */
	size_t currentIndex() const noexcept;

	/*!
	 * \brief Index of currently selected file among filtered files.
	 * \return FileQueue::npos Filtered queue is empty
	 */
	size_t currentIndexFiltered() const noexcept;


	/*!
	 * \brief Find a file in queue.
	 * \param path Path to file.
	 *
	 * \return Index of \p path in queue.
	 * \retval FileQueue::npos \p path was not found.
	 */
	size_t find(const QString& path) noexcept;


	/*!
	 * \brief Rename currently selected file.
	 * \param filename New file name.
	 *
	 * \returns Result of rename operation (see \ref RenameResult).
	 */
	RenameResult renameCurrentFile(const QString& filename);


	/*!
	 * \brief Delete currently selected file permanently.
	 *
	 * \retval true  Deleted successfully.
	 * \retval false Could not delete file. Queue not modified.
	 */
	bool deleteCurrentFile();


	/*!
	 * \brief Erase currently selected file path from queue.
	 */
	void eraseCurrent();


	/*!
	 * \brief Serialize file names in queue to a binary file at \p path.
	 * \return Number of bytes written.
	 */
	size_t saveToFile(const QString& path) const;


	/*!
	 * \brief Assign file names into queue from specified file.
	 *        Previous contents of queue is lost.
	 * \return Number of entries added to queue.
	 */
	size_t loadFromFile(const QString& path);

	/*!
	 * \brief Serialize file names in queue to a memory buffer.
	 */
	QByteArray saveToMemory() const;

	/*!
	 * \brief Assign file names into queue from \p memory buffer.
	 *        Previous contents of queue is lost.
	 * \return Number of entries added to queue.
	 */
	size_t loadFromMemory(const QByteArray& memory);

private:
	void update_filter();

	static const QString m_empty;
	std::deque<QString>  m_files;
	std::vector<bool>    m_accepted_by_filter;
	QStringList          m_name_filters;
	QStringList          m_substr_filter_include;
	QStringList          m_substr_filter_exclude;
	size_t               m_current = npos;
	ptrdiff_t            m_accepted_by_filter_count = -1;
	SortQueueBy          m_sort_by = SortQueueBy::FileName;
};

#endif
