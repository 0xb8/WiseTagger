/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "file_queue.h"
#include <util/debug.h>
#include <QDirIterator>
#include <QCollator>
#include "util/traits.h"

const QString FileQueue::m_empty{nullptr,0};

void FileQueue::setNameFilter(const QStringList &f)
{
	m_name_filters = f;
}

bool FileQueue::checkExtension(const QString &ext) noexcept
{
	for(const auto & f : m_name_filters) {
		if(f.endsWith(ext)) return true;
	}
	return false;
}

void FileQueue::push(const QString &f)
{
	QFileInfo fi(f);
	if(!fi.exists()) {
		pwarn << "attempted to push() unexistent file";
		return;
	}

	if(fi.isFile() && checkExtension(fi.suffix())) {
		m_files.push_back(fi.absoluteFilePath());

	} else if(fi.isDir()) {
		QDirIterator it(f, m_name_filters, QDir::Files | QDir::Readable);
		while(it.hasNext() && !it.next().isNull()) {
			m_files.push_back(it.fileInfo().absoluteFilePath());
		}
	} else {
		pwarn << "invalid extension";
	}
}

const QString& FileQueue::select(size_t index)
{
	if(index >= m_files.size())
		return m_empty;

	const auto& ret = m_files[index]; // let the exception escape before modifying state
	m_current = index;
	return ret;
}

void FileQueue::sort() noexcept
{
	static_assert(util::traits::is_nothrow_swappable_all_v<decltype(m_files)::value_type>, "");

	if(m_files.empty() || m_files.size() == 1)
		return;

	QCollator collator;
	collator.setNumericMode(true);

	std::sort(m_files.begin(), m_files.end(),
		[&collator](const auto& a, const auto& b)
		{
			return collator.compare(a,b) < 0;
		});

	m_files.erase(std::unique(m_files.begin(), m_files.end()), m_files.end());
	if(m_current >= m_files.size()) // in case duplicates were actually erased
		m_current = 0;
}

bool FileQueue::renameCurrentFile(const QString& new_path)
{
	if(m_current >= m_files.size()) {
		return false;
	}

	bool renamed = QFile(m_files.at(m_current)).rename(new_path);
	if(renamed) {
		m_files.at(m_current) = new_path;
	}
	return renamed;
}

bool FileQueue::deleteCurrentFile()
{
	if(m_current >= m_files.size()) {
		return false;
	}
	auto filename = m_files[m_current];
	QFile file(filename);

	bool removed = file.remove();
	if(removed) {
		eraseCurrent();
	}
	return removed;

}

void FileQueue::eraseCurrent()
{
	if(m_current >= m_files.size()) {
		return;
	}

	m_files.erase(std::next(std::begin(m_files), m_current));
	if(m_current >= m_files.size())
		m_current = 0u;
}

const QString& FileQueue::forward()
{
	if(m_current >= m_files.size()) {
		return m_empty;
	}
	if(++m_current >= m_files.size()) {
		m_current = 0u;
	}
	return m_files[m_current];
}

const QString& FileQueue::backward()
{
	if(m_current >= m_files.size()) {
		return m_empty;
	}
	if(m_current == 0) {
		m_current = m_files.size();
	}
	--m_current;
	return m_files[m_current];
}

size_t FileQueue::find(const QString &file) noexcept
{
	QFileInfo fi(file);
	Q_ASSERT(fi.exists() && fi.isFile());

	auto pos = std::find(m_files.begin(), m_files.end(), fi.absoluteFilePath());
	if(pos == m_files.end()) {
		return FileQueue::npos;
	}
	m_current = std::distance(m_files.begin(), pos);
	return m_current;
}

const QString &FileQueue::current() const
{
	if(m_current >= m_files.size()) {
		return m_empty;
	}
	return m_files[m_current]; // no need for another bounds-check
}

size_t FileQueue::currentIndex() const noexcept
{
	static_assert(noexcept(m_files.size()), "");
	if(m_current >= m_files.size()) {
		return FileQueue::npos;
	}
	return m_current;
}

bool FileQueue::empty() const noexcept
{
	static_assert(noexcept(m_files.empty()), "");
	return m_files.empty();
}

size_t FileQueue::size() const noexcept
{
	static_assert(noexcept(m_files.size()), "");
	return m_files.size();
}

void FileQueue::clear() noexcept
{
	static_assert(noexcept(m_files.clear()), "");
	m_files.clear();
	m_current = 0u;
}

