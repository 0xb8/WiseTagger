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

	QWriteLocker _{&m_lock};
	QPixmapCache::setCacheLimit(size_in_kb);
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

ImageCache::QueryResult ImageCache::getPixmap(const QString& filename)
{
	ImageCache::Key k;
	QueryResult res;
	res.result = State::Invalid;

	QReadLocker _{&m_lock};
	auto p = m_filekeys.find(filename);
	if(p != m_filekeys.end()) {
		k = p->second;
		// only query cache if we know image was loaded at some point
		if(k.state == State::Ready && QPixmapCache::find(k.key, &res.pixmap)) {
			res.original_size = k.original_size;
		}
		res.result = k.state;
	}
	return res;
}

void ImageCache::addFileThreadFunc(QString filename, QSize window_size)
{
	bool evicted = false;
	{
		QReadLocker _{&m_lock};
		auto p = m_filekeys.find(filename);
		if(p != m_filekeys.end()) {
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
		QWriteLocker _{&m_lock};
		auto p = m_filekeys.find(filename);
		if(p != m_filekeys.end()) {
			// NOTE: another thread might have finished reloading file, better check again
			if(p->second.state == State::Ready && !QPixmapCache::find(p->second.key, nullptr)) {
				pwarn << "pixmap evicted, reloading: " << filename;
				p->second.state = State::Evicted;
			}
			else return; // someone already reloaded image
		} else {
			pcrit << "entry lost while trying to reload evicted image";
			return;
		}
	}

	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly)) {
		QWriteLocker _{&m_lock};
		auto p = m_filekeys.find(filename);
		if(p != m_filekeys.end()) {
			p->second.state = State::Invalid;
		}
		return;
	}

	QImageReader reader(&file);
	if(!reader.canRead() || reader.supportsAnimation()) { // NOTE: to prevent caching of animated images
		QWriteLocker _{&m_lock};
		auto p = m_filekeys.find(filename);
		if(p != m_filekeys.end()) {
			p->second.state = State::Invalid;
		}
		return;
	}

	{
		QWriteLocker _{&m_lock};
		// reserve entry in map for this image
		auto res = m_filekeys.emplace(std::make_pair(filename, Key{QPixmapCache::Key{}, QSize{}, State::Loading}));
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

	auto pixmap = QPixmap::fromImageReader(&reader);
	file.close();
	QSize original_size = pixmap.size();

	float ratio = std::min(window_size.width()  / static_cast<float>(pixmap.width()),
	                       window_size.height() / static_cast<float>(pixmap.height()));

	QPixmap respixmap;
	if(ratio < 1.0f) {
		respixmap = pixmap.scaled(QSize(pixmap.width() * ratio, pixmap.height() * ratio),
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
	} else {
		respixmap = pixmap;
	}
	pdbg << "loaded" << (ratio < 1.0f ? "and resized" : "") << "pixmap for" << filename << "of" << respixmap.size();

	QWriteLocker _{&m_lock};
	auto p = m_filekeys.find(filename);
	if(p != m_filekeys.end()) {
		if(p->second.state != State::Loading) {
			pcrit << "attempted to finalize pixmap that was not being loaded";
			return;
		}
		p->second.key = QPixmapCache::insert(respixmap);
		p->second.original_size = original_size;
		p->second.state = State::Ready;
	} else {
		pwarn << "reserved entry lost, recreating...";
		auto res = m_filekeys.emplace(std::make_pair(std::move(filename), Key{QPixmapCache::insert(respixmap), original_size, State::Ready}));
		if(!res.second) {
			pcrit << "Could not recreate entry!";
		}
	}
}

void ImageCache::clear()
{
	QWriteLocker _{&m_lock};
	m_filekeys.clear();
	QPixmapCache::clear();
}


