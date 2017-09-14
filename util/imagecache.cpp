/* Copyright © 2017 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "imagecache.h"
#include <QFile>
#include <QImageReader>
#include <QLoggingCategory>
#include <QtConcurrent/QtConcurrentRun>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <QDir>

static uint64_t get_file_identifier(const QString& path)
{
	auto native_path = QDir::toNativeSeparators(path);
	uint64_t ret = 0;

	HANDLE h = CreateFileW((wchar_t*)native_path.utf16(),
	                       0,
	                       FILE_SHARE_DELETE,
	                       nullptr,
	                       OPEN_EXISTING,
	                       FILE_ATTRIBUTE_NORMAL,
	                       nullptr);

	if(h == INVALID_HANDLE_VALUE)
		return 0u;

	BY_HANDLE_FILE_INFORMATION info;
	if(GetFileInformationByHandle(h, &info)) {
		uint64_t inode = ((uint64_t)info.nFileIndexHigh << 32) | (uint64_t)info.nFileIndexLow;
		uint64_t device = (uint64_t)info.dwVolumeSerialNumber << 32;
		uint64_t mtime = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) | (uint64_t)info.ftLastWriteTime.dwLowDateTime;
		uint64_t mtime_posix = mtime / 10000 - 11644473600ll; // close enough POSIX conversion
		ret = inode ^ device ^ (mtime_posix << 16);
	}
	CloseHandle(h);
	return ret;
}
#elif defined(Q_OS_UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t get_file_identifier(const QString &path)
{
	auto native_path = path.toLocal8Bit();
	uint64_t ret = 0u;

	struct stat sb;
	if(0 == stat(native_path.constData(), &sb)) {
		uint64_t inode = sb.st_ino;
		uint64_t device = sb.st_dev;
		uint64_t mtime = ((uint64_t)sb.st_mtim.tv_sec); // nanosec timestamps not supported by ntfs-3g
		ret = inode ^ (device << 32) ^ (mtime << 16);
	}
	return ret;
}
#else
#error "get_file_identifier() not implemented for this OS"
#endif

namespace logging_category {Q_LOGGING_CATEGORY(imagecache, "ImageCache")}
#define pdbg qCDebug(logging_category::imagecache)
#define pwarn qCWarning(logging_category::imagecache)
#define pcrit qCCritical(logging_category::imagecache)

#define FUTURES_COUNT 40
#define DEFAULT_CACHE_SIZE_KB 64*1024

struct ImageCache::futures
{
	std::array<QFuture<void>, FUTURES_COUNT> ft;
};

ImageCache::ImageCache()
{
	QPixmapCache::setCacheLimit(DEFAULT_CACHE_SIZE_KB);
	m_file_id_cache.reserve(DEFAULT_CACHE_SIZE_KB / 512);
	m_id_keys.reserve(DEFAULT_CACHE_SIZE_KB / 512);
	m_futures = new futures;
}

ImageCache::~ImageCache()
{
	pdbg << "waiting on remaining futures...";
	for(auto& f : m_futures->ft) {
		f.waitForFinished();
	}
	delete m_futures;
	pdbg << "waiting on remaining futures completed.";
}

void ImageCache::setMemoryLimit(size_t size_in_kb)
{
	if(size_in_kb < DEFAULT_CACHE_SIZE_KB) {
		pwarn.nospace() <<"memory limit (" << size_in_kb << " KiB) is smaller than default (" << DEFAULT_CACHE_SIZE_KB << " KiB); did you forget to convert to kilobytes?";
		return;
	}

	{
		QWriteLocker _{&m_file_id_cache_lock};
		m_file_id_cache.reserve(size_in_kb / 512);
	}
	{
		QWriteLocker _{&m_id_keys_lock};
		m_id_keys.reserve(size_in_kb / 512);
		QPixmapCache::setCacheLimit(size_in_kb);
	}
}

void ImageCache::addFile(const QString& filename, QSize window_size)
{
	int available_idx = -1;
	for(int i = 0; i < FUTURES_COUNT; ++i) {
		auto f = m_futures->ft[i];
		if(f.isCanceled() || f.isFinished()) {
			available_idx = i;
			break;
		}
	}
	if(available_idx < 0) {
		pwarn << "too many pending futures";
		return;
	}

	auto future = QtConcurrent::run(this, &ImageCache::addFileThreadFunc, filename, window_size);

	m_futures->ft[available_idx] = future;
}

ImageCache::QueryResult ImageCache::getPixmap(const QString& filename, uint64_t unique_id) const
{
	ImageCache::Key k;
	QueryResult res;
	res.result = State::Invalid;
	res.unique_id = 0u;

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
			res.unique_id = get_file_identifier(filename);
		}
		unique_id = res.unique_id;
	} else {
		res.unique_id = unique_id;
	}

	if(res.unique_id == 0) { // file probably does not exist
		return res;
	}

	QReadLocker _{&m_id_keys_lock};
	auto key_it = m_id_keys.find(unique_id);
	if(key_it != m_id_keys.end()) {
		k = key_it->second;
		// only query cache if we know image was loaded at some point
		if(k.state == State::Ready) {
			if(QPixmapCache::find(k.key, &res.pixmap) && !res.pixmap.isNull()) {
				res.original_size = k.original_size;
			} else {
				k.state = State::Invalid;
			}
		}
		res.result = k.state;
	}
	return res;
}

void ImageCache::addFileThreadFunc(QString filename, QSize window_size)
{
	auto file_id = getUniqueFileID(filename);
	bool evicted = false;
	{
		QReadLocker _{&m_id_keys_lock};
		auto p = m_id_keys.find(file_id);
		if(p != m_id_keys.end()) {
			switch (p->second.state)
			{
			case State::Loading:
			case State::Invalid:
			case State::Evicted:
				return;

			case State::Ready:
				if(QPixmapCache::find(p->second.key, nullptr))
				{
					return; // already in cache
				}
				evicted = true;
				break;
			}
		}
	}

	if(evicted) {
		QWriteLocker _{&m_id_keys_lock};
		auto p = m_id_keys.find(file_id);
		if(p != m_id_keys.end()) {
			// NOTE: another thread might have finished reloading file, better check again
			if(p->second.state == State::Ready && !QPixmapCache::find(p->second.key, nullptr)) {
				pdbg << "pixmap evicted, reloading: " << filename;
				p->second.state = State::Evicted;
			}
			else return; // someone already reloaded image
		} else {
			pcrit << "entry lost while trying to reload evicted image";
			return;
		}
	}

	{
		QWriteLocker _{&m_id_keys_lock};
		// reserve entry in map for this image
		auto res = m_id_keys.emplace(std::make_pair(file_id, Key{QPixmapCache::Key{}, QSize{}, State::Loading}));
		if(!res.second) { // already exists

			switch (res.first->second.state) {
			case State::Ready:
			case State::Loading:
			case State::Invalid:
				pdbg << "attempted to load image that is already being loaded";
				return;
			case State::Evicted: // only load if image was evicted
				res.first->second.state = State::Loading;
				break;
			}
		}
	}

	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly)) {
		setFileInvalid(file_id);
		return;
	}

	QImageReader reader(&file);
	if(!reader.canRead() || reader.supportsAnimation()) { // NOTE: to prevent caching of animated images
		setFileInvalid(file_id);
		return;
	}

	auto image = reader.read();
	file.close();

	if(image.isNull()) {
		setFileInvalid(file_id);
		return;
	}

	QSize original_size = image.size();
	float ratio = std::min(window_size.width()  / static_cast<float>(image.width()),
	                       window_size.height() / static_cast<float>(image.height()));
	QSize new_size(image.width() * ratio, image.height() * ratio);

	QPixmap respixmap;
	if(ratio < 1.0f) {
		respixmap = QPixmap::fromImage(image.scaled(new_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	} else {
		respixmap = QPixmap::fromImage(std::move(image));
	}
	pdbg << "loaded" << (ratio < 1.0f ? "and resized" : "")
	     << "pixmap for" << filename.mid(filename.lastIndexOf('/')+1) << "/" << file_id << "of" << new_size;

	{
		QWriteLocker _{&m_id_keys_lock};
		auto p = m_id_keys.find(file_id);
		if(p != m_id_keys.end()) {
			if(p->second.state != State::Loading) {
				pcrit << "attempted to finalize pixmap that was not being loaded";
				return;
			}

			p->second.key = QPixmapCache::insert(respixmap);
			p->second.original_size = original_size;
			p->second.state = State::Ready;
		}
	}
}

uint64_t ImageCache::getUniqueFileID(const QString& filename)
{
	uint64_t file_id = 0;
	{ // look for file id in cache first
		QReadLocker _{&m_file_id_cache_lock};
		auto id_it = m_file_id_cache.find(filename);
		if(id_it != m_file_id_cache.end()) {
			file_id = id_it->second;
		}
	}

	// could not find id in cache, have to query the filesystem
	if(file_id == 0) {
		file_id = get_file_identifier(filename);
		if(file_id != 0) {
			QWriteLocker _{&m_file_id_cache_lock};
			m_file_id_cache.insert(std::make_pair(filename, file_id));
		}
	}
	return file_id;
}

void ImageCache::setFileInvalid(uint64_t unique_id)
{
	QWriteLocker _{&m_id_keys_lock};
	auto p = m_id_keys.find(unique_id);
	if(p != m_id_keys.end()) {
		p->second.state = State::Invalid;
	}
}

void ImageCache::clear()
{
	{
		QWriteLocker _{&m_file_id_cache_lock};
		m_file_id_cache.clear();
	}
	{
		QWriteLocker _{&m_id_keys_lock};
		m_id_keys.clear();
		QPixmapCache::clear();
	}
}
