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
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QFileSystemWatcher>
#include <deque>
#include <memory>
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
class FileQueue : public QObject {
	Q_OBJECT
public:

	/// Construct FileQueue object.
	FileQueue();

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

	/// Extension filter for session files.
	static const QString sessionExtensionFilter;

	/// Set sorting criteria to be used by default.
	void setSortBy(SortQueueBy criteria) noexcept;

	/*!
	 * \brief Set file extensions filter used by FileQueue::push
	 *        and FileQueue::checkExtension
	 * \param filters List of extension wildcards, \em e.g. \code *.jpg \endcode
	 * \note Does NOT support multilevel extension wildcards, \em e.g. \code *.tar.gz \endcode
	 */
	void setExtensionFilter(const QStringList& filters) noexcept(false);


	/*!
	 * \brief Check if file's suffix is allowed by currently set extension filter.
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
	 * \param path      Path to file or directory.
	 * \param recursive When \p path refers to a directory,
	 *        enqueue files in all subdirectories recursively.
	 *
	 * When \p path refers to a file, it is added to queue.
	 *
	 * When \p path refers to a directory, files inside \p path
	 * are added to queue if their extension is accepted
	 * by current extension filter (see \ref FileQueue::setExtensionFilter())
	 *
	 * \note This function provides basic exception safety guarantee.
	 */
	void push(const QString& path, bool recursive);



	/*!
	 * \brief Replace queue contents with list of \p paths.
	 * \param paths List of file or directory paths.
	 * \param recursive When a directory is encountered in \p paths,
	 *        enqueue files in all subdirectories recursively.
	 *
	 * \note This function provides strong exception safety guarantee.
	 */
	void assign(const QStringList& paths, bool recursive);


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
	 * \note When no files match the filter, behaves as if the filter is not set.
	 *
	 * \return Next file.
	 * \retval EmptyString Queue is empty.
	 */
	const QString& forward() noexcept;


	/*!
	 * \brief Return next file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 * \note When no files match the filter, behaves as if the filter is not set.
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
	 * \note When no files match the filter, behaves as if the filter is not set.
	 *
	 * \return Previous file.
	 * \retval EmptyString Queue is empty.
	 */
	const QString& backward() noexcept;


	/*!
	 * \brief Return previous file in queue, possibly wrapping around.
	 *
	 * \note This function respects currently set substring filter (see \ref FileQueue::setSubstringFilter())
	 * \note When no files match the filter, behaves as if the filter is not set.
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
	 * \brief Does the current file match the substring filter.
	 */
	bool currentFileMatchesQueueFilter() const noexcept;


	/*!
	 * \brief Does the \p file match the substring filter.
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


	/*!
	 * \brief Returns a list of all unique directories in the queue.
	 *
	 * The order of directories is unspecified.
	 */
	QStringList allDirectories() const;

signals:

	/*!
	 * \brief Emitted when new files are externally added into one of the directories in queue.
	 */
	void newFilesAdded();

private:
	void update_filter();
	void on_watcher_directory_changed(const QString& dir);
	void process_changed_directory(const QString& dir);

	static const QString m_empty;
	static const int watcher_update_granularity_ms;
	std::deque<QString>  m_files;
	QHash<QString, QSet<QString>>  m_dir_files;
	std::unique_ptr<QFileSystemWatcher> m_dir_watcher;
	QSet<QString>        m_watcher_changed_dirs;
	QTimer               m_watcher_timer;
	std::vector<bool>    m_accepted_by_filter;
	QStringList          m_ext_filters;
	QStringList          m_substr_filter_include;
	QStringList          m_substr_filter_exclude;
	size_t               m_current = npos;
	ptrdiff_t            m_accepted_by_filter_count = -1;
	SortQueueBy          m_sort_by = SortQueueBy::FileName;
};

#endif
