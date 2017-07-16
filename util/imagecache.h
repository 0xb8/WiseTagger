/* Copyright © 2017 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QPixmapCache>
#include <QReadWriteLock>
#include <unordered_map>
#include "util/unordered_map_qt.h"

/*!
 * \brief Provides threaded pixmap prefetcher and resizer.
 *
 * This class provides multi-threaded image file preloading and resizing to
 * common display size.
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

	enum State
	{
		Invalid, ///< Failed to read or decode the image.
		Loading, ///< Image is currently being loaded by some thread
		Ready,   ///< Image is ready for use.
		Evicted, ///< Image has been evicted from pixmap cache.
	};

	struct QueryResult
	{

		/// Valid pixmap if \p result is \a State::Ready, null pixmap otherwise.
		QPixmap           pixmap;

		/// Original size of pixmap if \p result is \a State::Ready, invalid size otherwise.
		QSize             original_size;

		/// Non-zero unique id if \p result is not \a State::Invalid, zero otherwise.
		uint64_t          unique_id;

		/// Result of cache query.
		ImageCache::State result;
	};

	/// Clears cache.
	void    clear();

	/// Sets maximum memory pixmap cache can use.
	void    setMemoryLimit(size_t size_in_kb);

	/*!
	 * \brief Adds file to be preloaded and resized according to \p window_size.
	 * \param filename File to preload
	 * \param window_size Used to determine the size of cached image
	 *
	 * This function schedules separate thread to load and resize image.
	 * You can safely issue multiple calls with same \p filename only if
	 * \p window_size remains the same for all calls. Otherwise the resulting
	 * pixmap size is unspecified.
	 */
	void    addFile(const QString& filename, QSize window_size);


	/*!
	 * \brief Tries to retrieve pixmap from cache.
	 * \param filename Image file, the pixmap of which to retrieve from cache
	 * \param unique_id If provided, used to avoid filename lookup in cache and/or filesystem.
	 */
	QueryResult getPixmap(const QString& filename, uint64_t unique_id = 0) const;


private:

	void addFileThreadFunc(QString filename, QSize window_size);
	void setFileInvalid(uint64_t unique_id);

	uint64_t getUniqueFileID(const QString& filename);

	struct Key
	{
		QPixmapCache::Key key;
		QSize original_size;
		State state;
	};
	struct futures;
	futures* m_futures;

	std::unordered_map<QString, uint64_t> m_file_id_cache;
	std::unordered_map<uint64_t, Key>     m_id_keys;
	mutable QReadWriteLock m_file_id_cache_lock;
	mutable QReadWriteLock m_id_keys_lock;
};

#endif // IMAGECACHE_H
