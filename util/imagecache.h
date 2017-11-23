/* Copyright © 2017 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QCache>
#include <QImage>
#include <QReadWriteLock>
#include <QThreadPool>
#include <atomic>
#include "util/unordered_map_qt.h"

/*!
 * \brief Provides threaded image prefetcher and resizer.
 *
 * This class provides multi-threaded image file preloading and resizing to
 * preferred display size.
 *
 * After an image file has been added to cache, users can query if that file is
 * ready for use, or decide to wait until it is ready otherwise.
 *
 * Member functions of this class are thread-safe unless noted othewise.
 */
class ImageCache
{

public:
	ImageCache();
	~ImageCache();

	enum class State
	{
		Invalid, ///< Failed to read or decode the image.
		Loading, ///< Image is currently being loaded by some thread
		Ready,   ///< Image is ready for use.
	};

	struct QueryResult
	{
		/// Valid image if \p result is \a State::Ready, null image otherwise.
		QImage            image;

		/// Original size of image if \p result is \a State::Ready, invalid size otherwise.
		QSize             original_size;

		/// Non-zero unique id if \p result is not \a State::Invalid, zero otherwise.
		uint64_t          unique_id;

		/// Result of cache query.
		ImageCache::State result;
	};

	/// Clears cache.
	void    clear();

	/// Sets maximum amount of memory in KiB for image cache.
	void    setMemoryLimitKiB(size_t size_in_kb);

	/*!
	 * \brief Sets maximum number of concurrent image load tasks.
	 *
	 * NOTE: Must be called from only main thread!
	 */
	void    setMaxConcurrentTasks(int num);

	/*!
	 * \brief Schedules file for preloading with respect to \p window_size.
	 * \param filename File to preload
	 * \param window_size Used to determine the resulting size of image.
	 *
	 * This function schedules image load/resize task in a thread pool.
	 * You can safely issue multiple calls with same \p filename if
	 * \p window_size remains unchanged for all calls.
	 * Otherwise it will pollute the cache with multiple versions of same
	 * image. Increasing the size generally requires complete reload anyway
	 * (for image quality), so you should \ref clear() the cache on size change.
	 */
	void    addFile(const QString& filename, QSize window_size);

	/*!
	 * \brief Tries to retrieve image from cache.
	 * \param window_size Expected image size.
	 * \param unique_id If non-zero, used to avoid filename lookup as optimization.
	 */
	QueryResult getImage(const QString& filename, QSize window_size, uint64_t unique_id = 0) const;


private:
	friend struct LoadResizeImageTask;

	void addFileThreadFunc(const QString & filename, QSize window_size);
	void setFileInvalid(uint64_t unique_id);
	void insertResizedImage(uint64_t unique_id, QImage&& image, QSize original_size);

	uint64_t getUniqueImageID(const QString& filename, QSize size);

	struct Entry
	{
		Entry(QImage&& img, QSize size, State st) : image(std::move(img)), original_size(size), state(st) { }
		QImage image;
		QSize  original_size;
		State  state;
	};

	using FilenameIdCache = std::unordered_map<QString, uint64_t>;

	QThreadPool            m_thread_pool;
	FilenameIdCache        m_file_id_cache;
	QCache<uint64_t,Entry> m_image_cache;
	mutable QReadWriteLock m_file_id_cache_lock;
	mutable QReadWriteLock m_image_cache_lock;
	std::atomic_bool       m_shutting_down;
};

#endif // IMAGECACHE_H
