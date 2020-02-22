/* Copyright © 2020 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "tag_parser.h"
#include <algorithm>
#include <QLoggingCategory>
#include <QTextStream>

namespace logging_category {
	Q_LOGGING_CATEGORY(parser, "TagParser")
}
#define pdbg qCDebug(logging_category::parser)
#define pwarn qCWarning(logging_category::parser)

#include "util/imageboard.h"

TagParser::TagParser(QObject * _parent) : QObject(_parent) { }

QStringList TagParser::fixTags(TagEditState& state,
                               QString text,
                               FixOptions options) const
{
	for (auto& p : qAsConst(m_regexps)) {

		auto& re = p.first;
		auto& src = p.second;

		QRegularExpressionMatch m = re.match(text);
		if (m.hasMatch()) {

			auto capture_start = m.capturedStart();
			auto capture_end = m.capturedEnd();

			auto t = text;
			t.remove(capture_start, capture_end - capture_start);

			for (auto r : related_tags(state, src)) {
				for (auto capture_group : re.namedCaptureGroups()) {
					if (!capture_group.isEmpty()) {
						r.replace(capture_group, m.captured(capture_group));
					}
				}
				t.append(' ');
				t.append(r);
			}
			if (!t.isEmpty()) {
				text = t;
			}
		}
	}


	auto text_list = text.split(QChar(' '), QString::SkipEmptyParts);
	if(options.replace_imageboard_tags)
		ib::replace_imageboard_tags(text_list);

	if(options.restore_imageboard_tags)
		ib::restore_imageboard_tags(text_list);

	for(int i = 0; i < text_list.size(); ++i) {
		remove_if_unwanted(state, text_list[i]); // TODO: refactor using iterators
		text_list += replacement_tags(state, text_list[i]);
		text_list += related_tags(state, text_list[i]);
	}

	text_list.removeDuplicates();
	text_list.erase(
		std::remove_if(
			text_list.begin(),
			text_list.end(),
			[](const auto& s){ return s.isEmpty(); }),
		text_list.end());

	using tag_iterator = decltype(std::begin(text_list));

	tag_iterator tag, id;
	if(!ib::find_imageboard_tags(text_list.begin(), text_list.end(),tag,id)) {
		id = text_list.begin();
	} else {
		// sorting should begin from next tag
		std::advance(id, 1);
	}

	if(options.sort) {
		std::sort(id, text_list.end());
	}

	// if the author tag is present and should be forced to be first, move it first
	if(options.force_author_handle_first) {
		// the pattern matches to (not-a-space-tab-newline)@AA - (not-a-space-tab-newline)@ZZ;
		// the list is then prepended by those having ↑ and original entries of ↑ are removed
		text_list = text_list.filter(QRegularExpression(R"~(\[?[^ \t\n\r]+@[A-Z]{2}\]?)~")) + text_list;
		text_list.removeDuplicates();
	}

	return text_list;
}


QString TagParser::getComment(const QString & tag) const
{
	QString ret;
	auto it = m_comment_tooltips.find(tag);
	if (it != m_comment_tooltips.end()) {
		ret = it->second;
	}
	return ret;
}


const QStringList & TagParser::getAllTags() const
{
	return m_tags_from_file;
}


bool TagParser::loadTagData(const QByteArray& data)
{
	if(data.isEmpty()) {
		return false;
	}

	m_related_tags.clear();
	m_replaced_tags.clear();
	m_removed_tags.clear();
	m_comment_tooltips.clear();
	m_regexps.clear();

	QTextStream in(data);
	in.setCodec("UTF-8");
	auto tags = parse_tags_file(&in);

	tags.removeDuplicates();
	m_tags_from_file = tags;
	return true;
}


bool TagParser::hasTagFile() const
{
	return !m_tags_from_file.isEmpty();
}


//------------------------------------------------------------------------------


QStringList TagParser::parse_tags_file(QTextStream *input)
{
	QString current_line, main_tag, removed_tag, replaced_tag, mapped_tag, comment;
	QString regex_source;
	QStringList main_tags_list, removed_tags_list, replaced_tags_list, mapped_tags_list;

	auto allowed_in_tag = [](QChar c)
	{
		const char valid_chars[] = "`~!@$%^&*()[]_+;.";
		return c.isLetterOrNumber() ||
			std::find(std::begin(valid_chars),
			          std::end(valid_chars),
			          c.toLatin1()) != std::end(valid_chars);
	};

	auto allowed_within_tag = [](QChar c)
	{
		return c == '-';
	};

	auto allowed_in_file = [&allowed_in_tag](QChar c)
	{
		const char valid_chars[] = " \t-=:,\'\"";
		return	c.isLetterOrNumber() ||
			allowed_in_tag(c)    ||
			std::find(std::begin(valid_chars),
		                  std::end(valid_chars),
		                  c.toLatin1()) != std::end(valid_chars);
	};

	while(!input->atEnd()) {
		current_line = input->readLine().trimmed();

		// process line continuations
		while (current_line.endsWith('\\')) {
			current_line.chop(1);
			current_line = current_line.trimmed();
			current_line.append(input->readLine().trimmed());
		}

		if (current_line.isEmpty())
			continue;

		main_tag.clear();
		removed_tag.clear();
		replaced_tag.clear();
		mapped_tag.clear();
		comment.clear();

		removed_tags_list.clear();
		replaced_tags_list.clear();
		mapped_tags_list.clear();

		int first_parse_symbol_pos = 0;
		bool appending_main = true, removing_main = false, found_replace = false, found_mapped = false, found_comment = false;


		// if first symbol on line is a single quote, it's a regular expression
		if (current_line.at(0) == '\'') {

			int from = -1;
			int idx = -1;
			int last_actual_quote_pos = -1;

			// search for last non-escaped single quote
			while((idx = current_line.lastIndexOf('\'', from)) > 1) {

				if (current_line[idx - 1] != '\\') {
					last_actual_quote_pos = idx;
					break;
				}
				else {
					// if found escaped quote, search again from it's position
					from = idx - current_line.length() - 1;
				}
			}

			Q_ASSERT(last_actual_quote_pos > 1);

			regex_source = current_line.mid(1, last_actual_quote_pos-1);

			if (!regex_source.isEmpty()) {

				QRegularExpression regex(regex_source);

				if (regex.isValid()) {
					appending_main = false;
					m_regexps.push_back(QPair<QRegularExpression, QString>(regex, regex_source));
					main_tag = regex_source;
					first_parse_symbol_pos = last_actual_quote_pos - 1;
				} else {
					emit parseError(regex_source, regex.errorString(), regex.patternErrorOffset() + 1);
					continue;
				}

			}
		}


		for(int i = first_parse_symbol_pos; i < current_line.length(); ++i) {

			if(current_line[i] == QChar('#')) {
				found_comment = true;
				continue;
			}

			if(found_comment) {
				comment.append(current_line[i]);
				continue;
			}

			if(!allowed_in_file(current_line[i])) {
				break; // go to next line
			}

			if(appending_main) {
				if(allowed_in_tag(current_line[i]) || (!main_tag.isEmpty() && allowed_within_tag(current_line[i]))) {
					main_tag.append(current_line[i]);
					continue;
				}
			}

			if(removing_main) {
				if(allowed_in_tag(current_line[i]) || allowed_within_tag(current_line[i])) {
					removed_tag.append(current_line[i]);
					continue;
				}
			}

			if(found_replace) {
				if(allowed_in_tag(current_line[i]) || allowed_within_tag(current_line[i])) {
					replaced_tag.append(current_line[i]);
					continue;
				}
			}

			if(found_mapped) {
				if(allowed_in_tag(current_line[i]) || allowed_within_tag(current_line[i])) {
					mapped_tag.append(current_line[i]);
					continue;
				}
			}

			if(current_line[i] == '-' && main_tag.isEmpty() && regex_source.isEmpty()) {
				appending_main = false;
				found_mapped = false;
				found_replace = false;
				removing_main = true;
				continue;
			}

			if(current_line[i] == '=' && !main_tag.isEmpty()) {
				appending_main = false;
				removing_main = false;
				found_mapped = false;
				found_replace = true;
				continue;
			}

			if(current_line[i] == ':' && !main_tag.isEmpty()) {
				appending_main = false;
				removing_main = false;
				found_replace = false;
				found_mapped = true;
				continue;
			}

			if(current_line[i] == ',') {
				if(removing_main && !removed_tag.isEmpty()) {
					removed_tags_list.push_back(removed_tag);
					removed_tag.clear();
				}

				if(found_replace && !replaced_tag.isEmpty()) {
					replaced_tags_list.push_back(replaced_tag);
					replaced_tag.clear();
				}
				if(found_mapped && !mapped_tag.isEmpty()) {
					mapped_tags_list.push_back(mapped_tag);
					mapped_tag.clear();
				}
			}

		} // for

		if(!removed_tag.isEmpty()) {
			removed_tags_list.push_back(removed_tag);
		}

		if(!replaced_tag.isEmpty()) {
			replaced_tags_list.push_back(replaced_tag);
		}

		if(!mapped_tag.isEmpty()) {
			mapped_tags_list.push_back(mapped_tag);
		}

		if(!main_tag.isEmpty()) {
			main_tags_list.push_back(main_tag);
			if(!comment.isEmpty()) {
				comment = comment.trimmed();
				auto it = m_comment_tooltips.find(main_tag);
				if(it != m_comment_tooltips.end()) {
					it->second.append(QStringLiteral(", "));
					it->second.append(comment);
				} else {
					m_comment_tooltips.insert(std::make_pair(main_tag, comment));
				}
			}
		}

		for(const auto& remtag : qAsConst(removed_tags_list)) {
			m_removed_tags.insert(remtag);
		}

		for (const auto& repltag : qAsConst(replaced_tags_list)) {
			m_replaced_tags.insert(std::pair<QString,QString>(repltag, main_tag));
		}

		for(const auto& maptag : qAsConst(mapped_tags_list)) {
			m_related_tags.insert(std::pair<QString,QString>(main_tag,maptag));
		}
	}
	return main_tags_list;
}

QStringList TagParser::related_tags(TagEditState& state, const QString &tag) const
{
	QStringList relatives;
	auto related_tags_range = m_related_tags.equal_range(tag);
	bool found_tag_entry = related_tags_range.first != m_related_tags.end();

	/* Mark this tag as processed. This also prevents infinite looping.
	   If user decides to remove related tag(s), they will not be added again.
	 */
	if(found_tag_entry && state.needAddRelated(tag)) {
		std::for_each(related_tags_range.first, related_tags_range.second,
			[&relatives](const auto& p)
			{	/* Add all related tags for this tag */
				relatives.push_back(p.second);
			});
	}
	return relatives;
}

QStringList TagParser::replacement_tags(TagEditState& state, QString &tag) const
{
	QStringList replacements;
	auto replaced_tags_range = m_replaced_tags.equal_range(tag);
	bool found_tag_entry = replaced_tags_range.first != m_replaced_tags.end();

	/* Mark this tag as processed. This also prevents infinite looping.
	   If user decides to remove replaced tag(s) and enters original one(s),
	   they will not be replaced again.
	 */
	if(found_tag_entry && state.needReplace(tag)) {
		std::for_each(replaced_tags_range.first, replaced_tags_range.second,
			[&replacements](const auto& p)
			{	/* Add all replacement tags for this tag */
				replacements.push_back(p.second);
			});

		tag.clear(); /* Clear original tag */
	}
	return replacements;
}

void TagParser::remove_if_unwanted(TagEditState & state, QString &tag) const
{
	if(m_removed_tags.count(tag)) {
		/* Allow user to enter and keep tag which has been autoremoved */
		if(state.needRemove(tag)) {
			/* Next time this tag will not be removed */
			tag.clear();
		}
	}
}

void TagEditState::clear() {
	m_removed_tags.clear();
	m_replaced_tags.clear();
	m_related_tags.clear();
	m_added_tags.clear();
}

bool TagEditState::needRemove(const QString & tag)
{
	return m_removed_tags.insert(tag).second;
}

bool TagEditState::needAdd(const QString & tag)
{
	return m_added_tags.insert(tag).second;
}


bool TagEditState::needAddRelated(const QString & tag)
{
	return m_related_tags.insert(tag).second;
}

bool TagEditState::needReplace(const QString & tag)
{
	return m_replaced_tags.insert(tag).second;
}


