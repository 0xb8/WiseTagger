/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "file_queue.h"
#include "util/traits.h"
#include "util/misc.h"
#include <QLoggingCategory>
#include <QDirIterator>
#include <QTextStream>
#include <QCollator>
#include <QDateTime>
#include <QSaveFile>
#include <QFile>
#include <QBuffer>

namespace logging_category {
	Q_LOGGING_CATEGORY(filequeue, "FileQueue")
}
#define pdbg qCDebug(logging_category::filequeue)
#define pwarn qCWarning(logging_category::filequeue)

/* static data */
const QString FileQueue::m_empty{nullptr,0};
const QString FileQueue::sessionNameFilter = QStringLiteral("*.wt-session");

void FileQueue::setNameFilter(const QStringList &f) noexcept(false)
{
	m_name_filters = f;
}

void FileQueue::setSubstringFilter(const QStringList & filters)
{
	m_substr_filter_include.clear();
	m_substr_filter_exclude.clear();

	if (!filters.isEmpty()) {
		for (const auto& tag : filters) {
			if (tag.size() > 1 && (tag[0] == '!' || tag[0] == '-'))
				m_substr_filter_exclude.append(tag.mid(1));
			else
				m_substr_filter_include.append(tag);
		}
		m_substr_filter_exclude.sort();
		m_substr_filter_include.sort();

		m_substr_filter_exclude.erase(std::unique(m_substr_filter_exclude.begin(),
		                                          m_substr_filter_exclude.end()),
		                              m_substr_filter_exclude.end());
		m_substr_filter_include.erase(std::unique(m_substr_filter_include.begin(),
		                                          m_substr_filter_include.end()),
		                              m_substr_filter_include.end());
	}
	update_filter();
}

bool FileQueue::substringFilterActive() const
{
	return m_substr_filter_exclude.size() + m_substr_filter_include.size();
}

bool FileQueue::checkSessionFileSuffix(const QFileInfo &fi)
{
	return fi.suffix() == QStringLiteral("wt-session") || fi.filePath() == QStringLiteral("-");
}

void FileQueue::setSortBy(SortQueueBy criteria) noexcept
{
	m_sort_by = criteria;
}

bool FileQueue::checkExtension(const QFileInfo & fi) const noexcept
{
	const auto ext = fi.suffix();
	for(const auto & f : qAsConst(m_name_filters)) {
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

	if(fi.isFile() && checkExtension(fi)) {
		m_files.push_back(fi.absoluteFilePath());
		m_accepted_by_filter.push_back(fileMatchesFilter(fi));

	} else if(fi.isDir()) {
		QDirIterator it(f, m_name_filters, QDir::Files);
		while(it.hasNext() && !it.next().isNull()) {
			auto fi = it.fileInfo();
			m_files.push_back(fi.absoluteFilePath());
			m_accepted_by_filter.push_back(fileMatchesFilter(fi));
		}
	} else {
		pwarn << "push() : extension not allowed by filter:" << fi.fileName();
	}
}

void FileQueue::assign(const QStringList& paths)
{
#if __GNUC__ > 5 // NOTE: workaround for old gcc used by appveyor
	static_assert(util::traits::is_nothrow_swappable_all_v<decltype(m_files)>, "Member container should be nothrow swappable");
#endif

	using cont_t = decltype(m_files);
	cont_t tmp;
	QFileInfo fi;
	for(const auto & p : qAsConst(paths)) {
		fi = p;
		if(fi.isFile()) {
			tmp.push_back(fi.absoluteFilePath());
		}
		if(fi.isDir()) {
			QDirIterator it(p, m_name_filters, QDir::Files);
			while(it.hasNext() && !it.next().isNull()) {
				tmp.push_back(it.fileInfo().absoluteFilePath());
			}
		}
	}

	std::swap(tmp, m_files);
	m_current = FileQueue::npos;
	update_filter();
}

const QString& FileQueue::select(size_t index) noexcept
{
	static_assert(noexcept(m_files[index]), "");

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
		if(res == 0)
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

	auto get_filename_ref = [](const auto& s)
	{
		int i = s.size();
		while (i --> 0) {
			if (s[i] == '/' || s[i] == '\\')
				return s.midRef(i+1);
		}

		return s.midRef(0);
	};

	auto compare_lengths = [&collator, get_filename_ref](const auto& a, const auto& b)
	{
		auto a_fname = get_filename_ref(a);
		auto b_fname = get_filename_ref(b);

		if (a_fname.size() != b_fname.size())
			return a_fname.size() < b_fname.size();

		return collator.compare(a,b) < 0;
	};

	auto compare_tagcount = [&collator, get_filename_ref](const auto& a, const auto& b)
	{
		auto a_fname = get_filename_ref(a);
		auto b_fname = get_filename_ref(b);

		auto space_count = [](const auto& s) {
			int acc = 0;
			for (int i = 0; i < s.size() - 1; ++i) {
				if (s[i].isSpace() && !s[i+1].isSpace())
					++acc;
			}
			return acc;
		};

		int sa = space_count(a_fname);
		int sb = space_count(b_fname);

		if (sa != sb)
			return sa < sb;

		return collator.compare(a, b) < 0;
	};

	auto curr_file = QString(current());

	if(m_sort_by == SortQueueBy::FileName)
		std::sort(m_files.begin(), m_files.end(), compare_names);

	if(m_sort_by == SortQueueBy::FileType)
		std::sort(m_files.begin(), m_files.end(), compare_types);

	if (m_sort_by == SortQueueBy::FileNameLength)
		std::sort(m_files.begin(), m_files.end(), compare_lengths);

	if (m_sort_by == SortQueueBy::TagCount) {
		std::sort(m_files.begin(), m_files.end(), compare_tagcount);
	}

	if(m_sort_by == SortQueueBy::FileSize || m_sort_by == SortQueueBy::ModificationDate) {
		std::deque<QFileInfo> infos;
		for(const auto& f : m_files) {
			infos.push_back(QFileInfo{f});
		}
		if(m_sort_by == SortQueueBy::FileSize) {
			std::sort(std::begin(infos), std::end(infos), compare_sizes);
		}
		if(m_sort_by == SortQueueBy::ModificationDate) {
			std::sort(std::begin(infos), std::end(infos), compare_dates);
		}
		Q_ASSERT(m_files.size() == infos.size());
		for(size_t i = 0u; i < infos.size(); ++i) {
			m_files[i] = infos[i].filePath();
		}
	}

	select(find(curr_file));
	if(m_current >= m_files.size()) // in case duplicates were actually erased
		m_current = 0;

	update_filter();
}

FileQueue::RenameResult FileQueue::renameCurrentFile(const QString& new_path)
{
	if(m_current >= m_files.size()) {
		pwarn << "renameCurrentFile(): queue empty or index is out of bounds";
		return RenameResult::SourceFileMissing;
	}

	const auto source_name = m_files.at(m_current);
	QFile source_file(source_name);
	if(!source_file.exists())
		return RenameResult::SourceFileMissing;

	if(QFile::exists(new_path)) {
#ifdef Q_OS_WIN
		// TODO: better check for case-insensitive filesystem (MacOS?)
		// in case renaming only changed the case
		if (source_name.compare(new_path, Qt::CaseInsensitive) == 0) {
			if (0 == util::is_same_file(source_file.fileName(), new_path)) {
				// it's definitely the same file (was there any point even checking?)
				if(source_file.rename(new_path)) {
					m_files.at(m_current) = new_path;
					return RenameResult::Success;
				}
			}
		}
#endif
		return RenameResult::TargetFileExists;
	}

	if(source_file.rename(new_path)) {
		m_files.at(m_current) = new_path;
		return RenameResult::Success;
	}

	pwarn << "renameCurrentFile(): error: " << source_file.errorString();
	return RenameResult::GenericFailure;
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
	if (!m_accepted_by_filter.empty()) {
		bool was_accepted = m_accepted_by_filter[m_current];
		if (was_accepted)
			--m_accepted_by_filter_count;

		// remove the filter bit for this file as well
		m_accepted_by_filter.erase(std::next(std::begin(m_accepted_by_filter), m_current));
		Q_ASSERT(m_accepted_by_filter.size() == m_files.size());
	}

	if(m_current >= m_files.size())
		m_current = 0u;

	if (!m_accepted_by_filter.empty()) {
		// if newly selected file is not accepted by filter, choose next file that is accepted.
		if (!m_accepted_by_filter[m_current])
			forward();
	}
}

size_t FileQueue::saveToFile(const QString &path) const
{
	if(empty() || !checkSessionFileSuffix(path))
		return 0;

	QSaveFile f(path);
	bool opened = f.open(QIODevice::WriteOnly);
	if(!opened) {
		pwarn << "saveToFile(): could not open" << path << "for writing";
		return 0;
	}

	auto raw_data = saveToMemory();
	f.write(qCompress(raw_data, 8));
	const auto ret = f.size();
	f.commit();
	return ret;
}



size_t FileQueue::loadFromFile(const QString &path)
{
	QFile f;
	QFileInfo fi(path);
	QTextStream stream;
	QBuffer data_buf;
	int curr = 0, size = -1;
	if(checkSessionFileSuffix(fi) && fi.exists()) {
		f.setFileName(path);
		bool opened = f.open(QIODevice::ReadOnly);
		if(!opened) {
			pwarn << "loadFromFile(): could not open" << path << "for reading";
			return 0;
		}

		data_buf.setData(qUncompress(f.readAll()));
		data_buf.open(QIODevice::ReadOnly);
		stream.setDevice(&data_buf);

		stream >> size >> curr;
		if(size <= 0 || curr < 0 || curr >= size) {
			pwarn << "Invalid size / current index:" << curr << size;
			return 0;
		}

	} else if(path == QStringLiteral("-")) {
		bool opened = f.open(stdin, QIODevice::ReadOnly|QIODevice::Text, QFileDevice::AutoCloseHandle);
		if(!opened) {
			pwarn << "loadFromFile(): could not open stdin for reading";
			return 0;
		}
		stream.setDevice(&f);
	} else {
		return 0;
	}
	stream.setCodec("UTF-8");

	std::deque<QString> res;
	QString line;
	line.reserve(256);

	while (stream.readLineInto(&line)) {
		if(!line.isEmpty()) {
			fi.setFile(line);
			if(checkExtension(fi))
				res.push_back(fi.canonicalFilePath());
		}
	}

	if(!res.empty() && (size_t)curr < res.size()) {
		m_files = std::move(res);
		m_current = curr;
	} else {
		pwarn << "loadFromFile(): file list is empty or smaller than current file index";
	}

	return m_files.size();
}

QByteArray FileQueue::saveToMemory() const
{
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
	return raw_data;
}

size_t FileQueue::loadFromMemory(const QByteArray& memory)
{
	QTextStream stream{memory, QIODevice::ReadOnly};
	stream.setCodec("UTF-8");

	int curr = 0, size = -1;
	stream >> size >> curr;
	if(size <= 0 || curr < 0 || curr >= size) {
		pwarn << "Invalid size / current index:" << curr << size;
		return 0;
	}

	std::deque<QString> res;
	QString line;
	line.reserve(256);

	QFileInfo fi;
	while (stream.readLineInto(&line)) {
		if(!line.isEmpty()) {
			fi.setFile(line);
			if(checkExtension(fi))
				res.push_back(fi.canonicalFilePath());
		}
	}

	if(!res.empty() && (size_t)curr < res.size()) {
		m_files = std::move(res);
		m_current = curr;
	} else {
		pwarn << "loadFromFile(): file list is empty or smaller than current file index";
	}

	return m_files.size();
}

void FileQueue::update_filter()
{
	m_accepted_by_filter.clear();
	m_accepted_by_filter_count = -1;

	// filter is not set, nothing to do
	if (!substringFilterActive())
		return;

	m_accepted_by_filter_count = 0u;
	for (size_t i = 0; i < m_files.size(); ++i) {
		auto file = m_files[i];
		QFileInfo fi(file);
		bool accepted = fileMatchesFilter(fi);
		m_accepted_by_filter.push_back(accepted);
		m_accepted_by_filter_count += accepted;
	}
}

const QString& FileQueue::forward() noexcept
{
	static_assert(noexcept(m_files[0]), "");
	return next(m_current);
}

const QString& FileQueue::backward() noexcept
{
	static_assert(noexcept(m_files[0]), "");
	return prev(m_current);
}

const QString & FileQueue::next(size_t& from) const noexcept
{
	if(from >= m_files.size()) {
		pwarn << "forward(): queue empty or index is out of bounds";
		return m_empty;
	}

	if (!filteredEmpty()) {
		Q_ASSERT(m_accepted_by_filter.size() == m_files.size());

		size_t next_filtered = from;
		while(true) {

			++next_filtered;

			if (next_filtered >= m_files.size()) // wrap around
				next_filtered -= m_files.size();

			if (m_accepted_by_filter[next_filtered])
				break;

			// loop is guaranteed to finish, since at least one file is accepted by filter
		}

		from = next_filtered;
		return m_files[from];
	}


	if(++from >= m_files.size()) { // wrap around
		from = 0u;
	}
	return m_files[from];
}

const QString & FileQueue::prev(size_t& from) const noexcept
{
	if(from >= m_files.size()) {
		pwarn << "backward(): queue empty or index is out of bounds";
		return m_empty;
	}

	if (!filteredEmpty()) {
		Q_ASSERT(m_accepted_by_filter.size() == m_files.size());

		auto prev_filtered = static_cast<ptrdiff_t>(from);
		const auto count = static_cast<ptrdiff_t>(m_accepted_by_filter.size());

		while(true) {
			--prev_filtered;

			if (prev_filtered < 0) // wrap around
				prev_filtered += count;

			if (m_accepted_by_filter[prev_filtered])
				break;

			// loop is guaranteed to finish, since at least one file is accepted by filter
		}

		from = prev_filtered;
		return m_files[from];
	}


	if(from == 0) { // wrap around
		from = m_files.size();
	}
	--from;
	return m_files[from];
}

const QString &FileQueue::nth(ptrdiff_t index) noexcept
{
	static_assert(noexcept(m_files[0]), "");

	if(m_files.empty()) {
		pwarn << "nth(): queue empty";
		return m_empty;
	}

	if(index < 0)
		index = m_files.size() + index;

	return m_files[std::abs(index) % m_files.size()];
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

const QString &FileQueue::current() const noexcept
{
	static_assert(noexcept(m_files[0]), "");

	if(m_current >= m_files.size()) {
		pwarn << "current(): queue empty or index is out of bounds";
		return m_empty;
	}
	return m_files[m_current]; // no need for another bounds-check
}

bool FileQueue::currentFileMatchesQueueFilter() const noexcept
{
	if (!substringFilterActive())
		return true;

	Q_ASSERT(m_accepted_by_filter.size() == m_files.size());

	if (m_current >= m_files.size()) {
		pwarn << "currentFileMatchesQueueFilter(): queue empty or index is out of bounds";
		return false;
	}

	return m_accepted_by_filter[m_current];
}

bool FileQueue::fileMatchesFilter(const QFileInfo& file) const
{
	QString name = file.completeBaseName();

	auto contains_separate = [](const QString& name, const auto& tag)
	{
		Q_ASSERT(!tag.isEmpty());

		if (Q_UNLIKELY(name.size() < tag.size()))
		    return false;


		int tag_pos = 0;
		// check each instance of tag in name
		while ((tag_pos = name.indexOf(tag, tag_pos)) >= 0) {

			auto left = name.leftRef(tag_pos); // part of name before the tag
			auto right = name.midRef(tag_pos + tag.size()); // part of name immediately after the tag

			bool left_separate = left.isEmpty() || left.back().isSpace();
			bool right_separate = right.isEmpty() || right.front().isSpace();

			if (left_separate && right_separate) // found separate tag
				return true;

			// advance search position
			tag_pos += tag.size();
		}
		return false; // could not find separate tag
	};

	auto is_tag_quoted = [](const QString& tag) {
		return tag.startsWith('"') && tag.endsWith('"');
	};

	auto unquoted_tag = [](const QString& tag) {
		return tag.midRef(1, tag.size()-2);
	};


	// check if name contains excluded tags
	for (const auto& excl : qAsConst(m_substr_filter_exclude)) {

		// if tag is quoted, check if name contains separated tag
		if (is_tag_quoted(excl)) {

			if (contains_separate(name, unquoted_tag(excl)))
				return false;

		} else {
			if (name.contains(excl, Qt::CaseInsensitive))
				return false;
		}
	}

	// check if name contains all included tags
	int count = 0;
	for (const auto& incl : qAsConst(m_substr_filter_include)) {

		// if tag is quoted, check if name contains separated tag
		if (is_tag_quoted(incl)) {

			if (contains_separate(name, unquoted_tag(incl)))
				++count;

		} else {

			if (name.contains(incl, Qt::CaseInsensitive))
				++count;
		}
	}

	return count >= m_substr_filter_include.size();
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

size_t FileQueue::currentIndexFiltered() const noexcept
{
	if (m_accepted_by_filter.empty())
		return m_current;

	if (currentFileMatchesQueueFilter()) {
		auto index = std::count_if(m_accepted_by_filter.begin(),
					   std::next(m_accepted_by_filter.begin(), m_current),
					   [](auto&& v) { return v; });
		return index;
	}
	return m_current;
}

bool FileQueue::empty() const noexcept
{
	static_assert(noexcept(m_files.empty()), "");
	return m_files.empty();
}

bool FileQueue::filteredEmpty() const noexcept
{
	return m_accepted_by_filter_count <= 0;
}

size_t FileQueue::size() const noexcept
{
	static_assert(noexcept(m_files.size()), "");
	return m_files.size();
}

size_t FileQueue::filteredSize() const noexcept
{
	if (m_accepted_by_filter_count < 0)
		return m_files.size();

	return m_accepted_by_filter_count;

}

void FileQueue::clear() noexcept
{
	static_assert(noexcept(m_files.clear()), "");
	m_files.clear();
	m_accepted_by_filter.clear();
	m_current = 0u;
	m_accepted_by_filter_count = -1;
}
