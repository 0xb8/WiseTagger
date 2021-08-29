/* Copyright © 2017 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "imagecache.h"
#include "util/misc.h"
#include <QFile>
#include <QImageReader>
#include <QLoggingCategory>
#include <memory>
#include <limits>


static uint64_t get_image_identifier(const QString& filename, QSize size)
{
	uint64_t file_id = util::get_file_identifier(filename), image_id = 0u;
	if(file_id != 0) {
		image_id = file_id ^ (qHash(size.width(), 0x34ba7cf0) * qHash(size.height(), 0x98015dea));
	}
	return image_id;
}

namespace logging_category {Q_LOGGING_CATEGORY(imagecache, "ImageCache")}
#define pdbg qCDebug(logging_category::imagecache)
#define pwarn qCWarning(logging_category::imagecache)

#define DEFAULT_CACHE_SIZE_KB 64*1024

/// Task for loading and resizing an image in a thread pool.
struct LoadResizeImageTask : public QRunnable
{
	/// Pointer to image cache.
	ImageCache* cache;

	/// Image file path.
	QString     filename;

	/// Window size to calculate target image size.
	QSize       window_size;

	/// Window device pixel ratio.
	double      device_pixel_ratio = 1.0;

	/// Constructs the task.
	LoadResizeImageTask(ImageCache* cache_, const QString& filename_, QSize window_size_, double dpr_) :
	        cache(cache_), filename(filename_), window_size(window_size_), device_pixel_ratio(dpr_)
	{
		Q_ASSERT(cache);
		setAutoDelete(true);
	}

	/// Called when task is started in a thread.
	void run() override
	{
		if(cache->m_shutting_down.load(std::memory_order_acquire))
			return;

		if(filename.isEmpty() || window_size.isNull())
			return;

		filename.detach();
		auto addFileThreadFn = &ImageCache::addFileThreadFunc;
		(cache->*addFileThreadFn)(filename, window_size, device_pixel_ratio);
	}
};

ImageCache::ImageCache()
{
	m_file_id_cache.reserve(DEFAULT_CACHE_SIZE_KB / 512);
	m_image_cache.setMaxCost(DEFAULT_CACHE_SIZE_KB);
	m_shutting_down.store(false, std::memory_order_relaxed);
}

ImageCache::~ImageCache()
{
	m_shutting_down.store(true, std::memory_order_release);
	pdbg << "waiting on remaining tasks...";
	m_thread_pool.clear();
	m_thread_pool.waitForDone();
	pdbg << "waiting on remaining tasks completed.";
}

void ImageCache::setMemoryLimitKiB(size_t size_in_kb)
{
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	if(size_in_kb < DEFAULT_CACHE_SIZE_KB) {
		pwarn.nospace() <<"memory limit (" << size_in_kb << " KiB) is smaller than default (" << DEFAULT_CACHE_SIZE_KB << " KiB); did you forget to convert to kilobytes?";
		return;
	}

	{
		QWriteLocker _{&m_file_id_cache_lock};
		m_file_id_cache.reserve(size_in_kb / 256);
	}
	{
		QWriteLocker _{&m_image_cache_lock};
		m_image_cache.setMaxCost(size_in_kb * 1024);
	}
}

void ImageCache::setMaxConcurrentTasks(int num)
{
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	if(num <= 0) {
		m_thread_pool.setMaxThreadCount(QThread::idealThreadCount());
	} else {
		m_thread_pool.setMaxThreadCount(num);
	}
}

void ImageCache::addFile(const QString& filename, QSize window_size, double device_pixel_ratio)
{
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	auto task = std::make_unique<LoadResizeImageTask>(this, filename, window_size, device_pixel_ratio);
	if(m_thread_pool.tryStart(task.get())) {
		(void)task.release();
	}
}

ImageCache::QueryResult ImageCache::getImage(const QString& filename, QSize window_size, uint64_t unique_id) const
{
	QueryResult res;
	res.result = State::Invalid;
	res.unique_id = 0u;
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return res;

	if(unique_id == 0u) {
		QReadLocker _{&m_file_id_cache_lock};
		auto id_it = m_file_id_cache.find(filename);
		if(id_it != m_file_id_cache.end()) {
			res.unique_id = id_it->second;
		} else {
			/* We can't insert this unique_id back into cache without
			 * making the hashmap mutable, which could lead to
			 * hard-to-pinpoint problems with memory allocation.
			 *
			 * Hopefully OS does a better job caching file metadata
			 * than me :/
			 */
			res.unique_id = get_image_identifier(filename, window_size);
		}
		unique_id = res.unique_id;
	} else {
		res.unique_id = unique_id;
	}

	if(res.unique_id == 0) { // file probably does not exist
		return res;
	}

	QReadLocker _{&m_image_cache_lock};
	auto entry = m_image_cache.object(unique_id);
	if(entry) {
		// only query cache if we know image was loaded at some point
		if(entry->state == State::Ready) {
			if(entry->image.isNull()) {
				pdbg << "state is ready but image is null";
				entry->state = State::Invalid;
			} else {
				res.original_size = entry->original_size;
				res.image = entry->image;
				res.result = State::Ready;
			}
		} else {
			res.result = entry->state;
		}
	} else {
		pdbg << "evicted miss" << filename;
	}
	return res;
}

void ImageCache::addFileThreadFunc(const QString& filename, QSize window_size, double device_pixel_ratio)
{
	auto image_id = getUniqueImageID(filename, window_size);

	{	// check if other thread began to load image before locking for write
		QReadLocker _{&m_image_cache_lock};
		if(m_image_cache.contains(image_id))
			return;
	}

	{
		if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
			return;

		// reserve entry in cache for this image to indicate that this thread is already loading it
		auto entry = std::make_unique<Entry>(QImage{}, QSize{}, State::Loading);

		QWriteLocker _{&m_image_cache_lock};
		if(m_image_cache.contains(image_id)) // recheck
			return;

		if(m_image_cache.insert(image_id, entry.get())) {
			(void)entry.release();
		} else {
			pdbg << "failed to insert empty entry into cache:" << filename;
			return;
		}
	}

	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly)) {
		setFileInvalid(image_id);
		return;
	}

	auto format = util::guess_image_format(filename);

	QImageReader reader(&file, format);
	if(!reader.canRead() || reader.supportsAnimation()) { // NOTE: to prevent caching of animated images
		setFileInvalid(image_id);
		return;
	}

	auto image = reader.read();
	file.close();

	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	if(image.isNull()) {
		setFileInvalid(image_id);
		return;
	}

	QSize original_size = image.size();
	window_size *= device_pixel_ratio;
	float ratio = std::min(window_size.width() / static_cast<float>(image.width()),
	                       window_size.height() / static_cast<float>(image.height()));

	QSize new_size(image.width() * ratio, image.height() * ratio);

	QImage resimage;
	if(ratio < 1.0f) {
		resimage = image.scaled(new_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	} else {
		resimage = image;
	}
	resimage.setDevicePixelRatio(device_pixel_ratio);
	pdbg << "loaded" << (ratio < 1.0f ? "and resized" : "")
	     << "image for" << filename.mid(filename.lastIndexOf('/')+1) << "/" << image_id << "of" << new_size;

	insertResizedImage(image_id, std::move(resimage), original_size);
}

void ImageCache::insertResizedImage(uint64_t unique_id, QImage&& image, QSize original_size)
{
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0)) // NOTE: deprecated since Qt 5.10
	int cost = image.byteCount();
#else
	auto size = image.sizeInBytes();
	int cost = std::min(size, static_cast<decltype(size)>(std::numeric_limits<int>::max()));
#endif
	bool need_realloc = false;
	std::unique_ptr<Entry> entry;

	{ // fast path
		QWriteLocker _{&m_image_cache_lock};
		entry.reset(m_image_cache.take(unique_id));
		if(entry) {
			entry->image = std::move(image);
			entry->original_size = original_size;
			entry->state = State::Ready;
			if(m_image_cache.insert(unique_id, entry.get(), cost)) {
				(void)entry.release();
			}
		} else {
			need_realloc = true;
		}
	}

	if(need_realloc) { // slow path
		if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
			return;
		pdbg << "realloc required, slow path";
		entry = std::make_unique<Entry>(std::move(image), original_size, State::Ready);
		Q_ASSERT(entry);
		QWriteLocker _{&m_image_cache_lock};
		if(m_image_cache.insert(unique_id,entry.get(),cost)) {
			(void)entry.release();
		}
	}
}


uint64_t ImageCache::getUniqueImageID(const QString& filename, QSize size)
{
	uint64_t image_id = 0;
	{ // look for file id in cache first
		QReadLocker _{&m_file_id_cache_lock};
		auto id_it = m_file_id_cache.find(filename);
		if(id_it != m_file_id_cache.end()) {
			image_id = id_it->second;
		}
	}

	// could not find id in cache, have to query the filesystem
	if(image_id == 0) {
		image_id = get_image_identifier(filename, size);
		if(image_id) {
			QWriteLocker _{&m_file_id_cache_lock};
			m_file_id_cache.insert(std::make_pair(filename, image_id));
		}
	}
	return image_id;
}

void ImageCache::setFileInvalid(uint64_t unique_id)
{
	QWriteLocker _{&m_image_cache_lock};
	auto entry = m_image_cache.take(unique_id);
	if(entry) {
		entry->state = State::Invalid;
		entry->image = QImage{};
		entry->original_size = QSize{};
		m_image_cache.insert(unique_id, entry);
	}
}

void ImageCache::clear()
{
	if(Q_UNLIKELY(m_shutting_down.load(std::memory_order_acquire)))
		return;

	{
		QWriteLocker _{&m_file_id_cache_lock};
		m_file_id_cache.clear();
	}
	{
		QWriteLocker _{&m_image_cache_lock};
		m_image_cache.clear();
	}
}
