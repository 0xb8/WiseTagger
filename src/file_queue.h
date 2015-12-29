/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef FILE_QUEUE_H
#define FILE_QUEUE_H

#include <QStringList>
#include <QObject>
#include <deque>

class FileQueue : public QObject {
	Q_OBJECT
public:
	/*!
	 * \brief Value used as invalid index.
	 */
	static constexpr size_t npos = std::numeric_limits<size_t>::max();

	FileQueue()           = default;
	~FileQueue() override = default;

	FileQueue(const FileQueue&) = default;
	FileQueue(FileQueue&&)      = default;
	FileQueue& operator= (const FileQueue&) = default;
	FileQueue& operator= (FileQueue&&)      = default;


	/*!
	 * \brief Sets file extensions filter used by FileQueue::push and FileQueue::checkExtension
	 */
	void setNameFilter(const QStringList&);


	/*!
	 * \brief Checks if extension is allowed by currently set name filter.
	 * \param File path or extension.
	 */
	bool checkExtension(const QString&) noexcept;


	/*!
	 * \brief enqueues file or directory contents.
	 * \param path Path to file or directory.
	 *
	 * If \p path is a file, enqueues it. If \p path is a directory,
	 * enqueues files inside \p path, according to to filters set by
	 * FielQueue::setNameFilter
	 */
	void push(const QString& path);


	/*!
	 * \brief Sorts queue respecting natural numbering.
	 */
	void sort()  noexcept;


	/*!
	 * \brief Clears queue.
	 */
	void clear() noexcept;


	/*!
	 * \brief Selects next file in queue, possibly wrapping around.
	 * \return Empty string if queue is empty. Next file otherwise.
	 */
	const QString& forward();


	/*!
	 * \brief Selects previous file in queue, possibly wrapping around.
	 * \return Empty string if queue is empty. Previous file otherwise.
	 */
	const QString& backward();


	/*!
	 * \brief Selects file.
	 * \param Index to select.
	 * \return Empty string if index is out of bounds. Selected file otherwise.
	 */
	const QString& select(size_t);


	/*!
	 * \brief Returns current file.
	 * \return Empty string if queue is empty. Current file otherwise.
	 */
	const QString& current() const;


	/*!
	 * \brief Checks if queue is empty.
	 */
	bool   empty()        const noexcept;


	/*!
	 * \brief Returns number of enqueued files.
	 */
	size_t size()         const noexcept;


	/*!
	 * \brief Returns index of currently selected file.
	 * \return FileQueue::npos if queue is empty. Index of currently selected file otherwise.
	 */
	size_t currentIndex() const noexcept;


	/*!
	 * \brief Finds file in queue.
	 * \param path Absolute path to file.
	 * \return FileQueue::npos if \p path is not found in queue. Index of \p path otherwise.
	 */
	size_t find(const QString& path) noexcept;


	/*!
	 * \brief Renames currently selected file.
	 * \param New file name.
	 *
	 * \retval \c TRUE  Renamed successfully.
	 * \retval \c FALSE Could not rename.
	 */
	bool renameCurrentFile(const QString&);


	/*!
	 * \brief Deletes currently selected file permanently.
	 *
	 * \retval \c TRUE  Deleted successfully.
	 * \retval \c FALSE Could not delete file. Queue was not modified.
	 */
	bool deleteCurrentFile();


	/*!
	 * \brief Erases currently selected file from queue.
	 */
	void eraseCurrent();

private:
	static const QString m_empty;
	std::deque<QString>  m_files;
	QStringList          m_name_filters;
	size_t               m_current = std::numeric_limits<size_t>::max();
};

#endif
