/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "file_queue.h"
#include "util/traits.h"
#include <QLoggingCategory>
#include <QDirIterator>
#include <QTextStream>
#include <QCollator>
#include <QDateTime>
#include <QFile>

namespace logging_category {
	Q_LOGGING_CATEGORY(filequeue, "FileQueue")
}
#define pdbg qCDebug(logging_category::filequeue)
#define pwarn qCWarning(logging_category::filequeue)

/* static data */
const QString FileQueue::m_empty{nullptr,0};
const QString FileQueue::sessionFileSuffix = QStringLiteral("wt-session");
const QString FileQueue::sessionNameFilter = QStringLiteral("*.wt-session");

void FileQueue::setNameFilter(const QStringList &f)
{
	m_name_filters = f;
}

void FileQueue::setSortBy(FileQueue::SortBy criteria)
{
	m_sort_by = criteria;
}

bool FileQueue::checkExtension(const QString &ext) noexcept
{
	for(const auto & f : m_name_filters) {
		if(f.endsWith(ext, Qt::CaseInsensitive)) return true;
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
		pwarn << "push() : extension not allowed by filter:" << fi.fileName();
	}
}

const QString& FileQueue::select(size_t index)
{
	if(index >= m_files.size()) {
		pwarn << "select() index out of bounds";
		return m_empty;
	}

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

	auto compare_names = [&collator](const auto& a, const auto& b)
	{
		return collator.compare(a,b) < 0;
	};

	auto compare_types = [&collator](const auto&a, const auto& b)
	{
		const auto suff_a = a.midRef(a.lastIndexOf('.'));
		const auto suff_b = b.midRef(b.lastIndexOf('.'));
		const auto res = suff_a.compare(suff_b, Qt::CaseInsensitive);
		if(res == 0 )
			return collator.compare(a,b) < 0;
		return res < 0;
	};

	auto compare_sizes = [&collator](const auto& a, const auto& b)
	{
		if(a.size() == b.size())
			return collator.compare(a.filePath(), b.filePath()) < 0;
		return a.size() < b.size();
	};

	auto compare_dates = [&collator](const auto& a, const auto& b)
	{
		const auto amod = a.lastModified();
		const auto bmod = b.lastModified();
		if(amod == bmod)
			return collator.compare(a.filePath(), b.filePath()) < 0;
		return amod < bmod;
	};

	auto curr_file = QString(current());

	if(m_sort_by == SortBy::FileName)
		std::sort(m_files.begin(), m_files.end(), compare_names);

	if(m_sort_by == SortBy::FileType)
		std::sort(m_files.begin(), m_files.end(), compare_types);

	if(m_sort_by == SortBy::FileSize || m_sort_by == SortBy::ModificationDate) {
		std::deque<QFileInfo> infos;
		for(const auto& f : m_files) {
			infos.push_back(QFileInfo{f});
		}
		if(m_sort_by == SortBy::FileSize) {
			std::sort(std::begin(infos), std::end(infos), compare_sizes);
		}
		if(m_sort_by == SortBy::ModificationDate) {
			std::sort(std::begin(infos), std::end(infos), compare_dates);
		}
		Q_ASSERT(m_files.size() == infos.size());
		for(size_t i = 0u; i < infos.size(); ++i) {
			m_files[i] = infos[i].filePath();
		}
	}

	m_files.erase(std::unique(m_files.begin(), m_files.end()), m_files.end());
	select(find(curr_file));
	if(m_current >= m_files.size()) // in case duplicates were actually erased
		m_current = 0;
}

bool FileQueue::renameCurrentFile(const QString& new_path)
{
	if(m_current >= m_files.size()) {
		pwarn << "renameCurrentFile(): queue empty or index is out of bounds";
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
		pwarn << "deleteCurrentFile(): queue empty or index is out of bounds";
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
		pwarn << "eraseCurrent(): queue empty or index is out of bounds";
		return;
	}

	m_files.erase(std::next(std::begin(m_files), m_current));
	if(m_current >= m_files.size())
		m_current = 0u;
}

size_t FileQueue::saveToFile(const QString &path) const
{
	if(empty() || !path.endsWith(FileQueue::sessionFileSuffix))
		return 0;

	QFile f(path);
	bool opened = f.open(QIODevice::WriteOnly);
	if(!opened) {
		pwarn << "saveToFile(): could not open" << path << "for writing";
		return 0;
	}

	QByteArray raw_data;
	raw_data.reserve(m_files.size() * 128);
	QTextStream stream(&raw_data, QIODevice::WriteOnly);
	stream.setCodec("UTF-8");

	stream << m_files.size() << '\n' << m_current << '\n';


	for(const auto& e : m_files) {
		stream << e << '\n';
		if(stream.status() != QTextStream::Ok) {
			pwarn << "TextStream bad status:" << stream.status() << " for: " << e;
			return 0;
		}
	}
	stream.flush();
	f.write(qCompress(raw_data, 8));
	return f.size();
}

size_t FileQueue::loadFromFile(const QString &path)
{
	if(!path.endsWith(FileQueue::sessionFileSuffix)) {
		return 0;
	}

	QFile f(path);
	bool opened = f.open(QIODevice::ReadOnly);
	if(!opened) {
		pwarn << "loadFromFile(): could not open" << path << "for reading";
		return 0;
	}

	QTextStream stream(qUncompress(f.readAll()), QIODevice::ReadOnly);
	stream.setCodec("UTF-8");

	QString line;
	line.reserve(256);
	clear();

	int curr = -1, size = -1;
	stream >> size >> curr;

	if(size <= 0 || curr < 0 || curr >= size) {
		pwarn << "Invalid size / current index:" << curr << size;
		return 0;
	}

	while (stream.readLineInto(&line)) {
		if(!line.isEmpty()) {
			m_files.push_back(line);
		}
	}

	if(!m_files.empty()) {
		m_current = curr;
	}

	return m_files.size();
}

const QString& FileQueue::forward()
{
	if(m_current >= m_files.size()) {
		pwarn << "forward(): queue empty or index is out of bounds";
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
		pwarn << "backward(): queue empty or index is out of bounds";
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
		pwarn << "current(): queue empty or index is out of bounds";
		return m_empty;
	}
	return m_files[m_current]; // no need for another bounds-check
}

size_t FileQueue::currentIndex() const noexcept
{
	static_assert(noexcept(m_files.size()), "");
	if(m_current >= m_files.size()) {
		pwarn << "currentIndex(): queue empty or index is out of bounds";
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
