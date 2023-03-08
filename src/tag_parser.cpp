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
#include <QSettings>
#include "util/strings.h"

namespace logging_category {
	Q_LOGGING_CATEGORY(parser, "TagParser")
}
#define pdbg qCDebug(logging_category::parser)
#define pwarn qCWarning(logging_category::parser)

#include "util/imageboard.h"


TagParser::TagParser(QObject * _parent) : QObject(_parent) { }


TagParser::FixOptions TagParser::FixOptions::from_settings()
{
	FixOptions opts;
	QSettings settings;
	opts.replace_imageboard_tags = settings.value(QStringLiteral("imageboard/replace-tags"), false).toBool();
	opts.restore_imageboard_tags = settings.value(QStringLiteral("imageboard/restore-tags"), true).toBool();
	opts.force_author_handle_first = settings.value(QStringLiteral("imageboard/force-author-first"), false).toBool();
	return opts;
}


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


	auto text_list = util::split(text);
	if(options.replace_imageboard_tags)
		ib::replace_imageboard_tags(text_list);

	if(options.restore_imageboard_tags)
		ib::restore_imageboard_tags(text_list);

	for(int i = 0; i < text_list.size(); ++i) {
		remove_if_unwanted(state, text_list[i]); // TODO: refactor using iterators
		text_list += replacement_tags(state, text_list[i]);
		text_list += related_tags(state, text_list[i]);
	}

	// remove tags by negation
	remove_explicit(state, text_list);

	// remove unknown if options say so
	auto is_unknown = [&options, this, &text_list](const auto& tag) {
		return options.remove_unknown && classify(tag, text_list) & TagKind::Unknown;
	};

	text_list.erase(
		std::remove_if(
			text_list.begin(),
			text_list.end(),
			[&is_unknown](const auto& s){
				return s.isEmpty() || is_negation_token(s[0]) || is_unknown(s);
			}),
		text_list.end());

	// order matters here, cannot use std::unique :(
	text_list.removeDuplicates();

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

TagParser::TagClassification TagParser::classify(QStringView tag, const QStringList& all_tags) const
{
	// check if tag is negated consequent for some other tag
	for (const auto& context_tag : qAsConst(all_tags)) {
		auto related_tags_range = m_related_tags.equal_range(context_tag);
		for (auto it = related_tags_range.first; it != related_tags_range.second; ++it) {
			// find negated consequent tags
			if (starts_with_negation_token(it->second)) {
				if (it->second.midRef(1) == tag) {
					return TagKind::Removed;
				}
			}
		}
	}

	auto pos = m_tags_classification.find(tag);
	if (pos != m_tags_classification.end()) {
		return pos->second;
	}
	return TagKind::Unknown;
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

#ifdef QT_GUI_LIB
QColor TagParser::getColor(const QString & tag) const
{
	QColor ret;
	auto it = m_tag_colors.find(tag);
	if (it != m_tag_colors.end()) {
		ret = it->second;
	}
	return ret;
}


QColor TagParser::getCustomKindColor(TagKind kind) const
{
	switch (kind) {
	case TagKind::Consequent:
		return m_custom_implication_color;
	case TagKind::Removed:
		return m_custom_removal_color;
	case TagKind::Replaced:
		return m_custom_replacement_color;
	default:
		break;
	}
	return QColor{};
}
#endif

QString TagParser::getReplacement(const QString & tag) const
{
	QString ret;
	auto it = m_replaced_tags.find(tag);
	if (it != m_replaced_tags.end()) {
		ret = it->second;
	}
	return ret;
}

bool TagParser::getConsequents(const QString & antecedent, std::unordered_set<QString>& consequents) const
{
	auto related_tags_range = m_related_tags.equal_range(antecedent);
	for (auto it = related_tags_range.first; it != related_tags_range.second; ++it)
		consequents.insert(it->second);

	return related_tags_range.first != related_tags_range.second;
}

const QStringList & TagParser::getAllTags() const
{
	return m_tags_from_file;
}


bool TagParser::loadTagData(const QByteArray& data)
{
	m_tags_from_file.clear();
	m_related_tags.clear();
	m_replaced_tags.clear();
	m_removed_tags.clear();
	m_comment_tooltips.clear();
	m_regexps.clear();
	m_tags_classification.clear();

#ifdef QT_GUI_LIB
	m_tag_colors.clear();
	m_custom_implication_color = QColor();
	m_custom_removal_color = QColor();
	m_custom_replacement_color = QColor();
#endif

	if(data.isEmpty()) {
		return false;
	}

	QTextStream in(data);
	in.setCodec("UTF-8");
	m_tags_from_file = parse_tags_file(&in);
	m_tags_from_file.removeDuplicates(); // cannot use std::unique here either, order matters
	return !m_tags_from_file.empty();
}


bool TagParser::hasTagFile() const
{
	return !m_tags_from_file.isEmpty();
}


bool TagParser::isTagRemoved(const QString & tag) const
{
	return m_removed_tags.find(tag) != m_removed_tags.end();
}


bool TagParser::is_negation_token(QChar ch) noexcept
{
	const auto negation_symbol = QChar(0x00AC);    // ¬
	return ch == '-' || ch == negation_symbol;
}

bool TagParser::starts_with_negation_token(const QString& str) noexcept
{
	return str.size() > 0 && is_negation_token(str[0]);
}


//------------------------------------------------------------------------------


QStringList TagParser::parse_tags_file(QTextStream *input)
{
	QString current_line, main_tag, removed_tag, replaced_tag, consequent_tag, comment;
	QString regex_source;
	QStringList main_tags_list, removed_tags_list, replaced_tags_list, consequent_tags_list;

#ifdef QT_GUI_LIB
	std::unordered_map<QString, QColor> custom_categories;
#endif

	auto allowed_in_tag = [](QChar c, bool within=false)
	{
		if (c.isLetterOrNumber())
			return true;

		if (within && c == '-')
			return true;

		if (auto latin = c.toLatin1()) {
			const char valid_chars[] = "`~!@$%^&*()[]_+;.";
			return std::find(std::begin(valid_chars),
			                 std::end(valid_chars),
			                 latin) != std::end(valid_chars);
		}

		return false;
	};

	auto is_implication_token = [](QChar ch) {
		const std::array<QChar, 6> implication_symbols {
			QChar(':'),
			QChar(0x2192), // →
			QChar(0x21D2), // ⇒
			QChar(0x21DB), // ⇛
			QChar(0x27F6), // ⟶
			QChar(0x27F9), // ⟹
		};

		return std::find(implication_symbols.begin(),
		                 implication_symbols.end(),
		                 ch) != implication_symbols.end();
	};

	auto is_replacement_token = [](QChar ch) {
		const std::array<QChar, 6> replacement_symbols{
			QChar('='),
			QChar(0x2190), // ←
			QChar(0x21D0), // ⇐
			QChar(0x21DA), // ⇚
			QChar(0x27F5), // ⟵
			QChar(0x27F8), // ⟸
		};

		return std::find(replacement_symbols.begin(),
		                 replacement_symbols.end(),
		                 ch) != replacement_symbols.end();
	};

	auto allowed_in_file = [&allowed_in_tag,
	                       &is_implication_token,
	                       &is_replacement_token](QChar c)
	{
		if (allowed_in_tag(c, true))
			return true;

		if (is_implication_token(c) || is_replacement_token(c) || is_negation_token(c))
			return true;

		if (auto latin = c.toLatin1()) {
			const char valid_chars[] = " \t-=:,\'\"";
			return std::find(std::begin(valid_chars),
			                 std::end(valid_chars),
			                 latin) != std::end(valid_chars);
		}

		return false;
	};

	auto add_tag_kind = [this](const auto& tag, TagKind kind)
	{
		Q_ASSERT(kind != TagKind::Unknown);
		auto result = m_tags_classification.emplace(tag, kind);
		if (!result.second) {
			result.first->second |= kind;
		}
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

#ifdef QT_GUI_LIB
		if (parse_pragma(current_line, custom_categories))
			continue;
#endif

		main_tag.clear();
		removed_tag.clear();
		replaced_tag.clear();
		consequent_tag.clear();
		comment.clear();
		regex_source.clear();

		removed_tags_list.clear();
		replaced_tags_list.clear();
		consequent_tags_list.clear();

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


		for(int current_token_pos = first_parse_symbol_pos; current_token_pos < current_line.length(); ++current_token_pos) {
			const auto current_token = current_line[current_token_pos];

			if(current_token == QChar('#') && !found_comment) {
				found_comment = true;
				continue;
			}

			if(found_comment) {
				comment.append(current_token);
				continue;
			}

			if(!allowed_in_file(current_token)) {
				break; // go to next line
			}

			if(appending_main) {
				if(allowed_in_tag(current_token, !main_tag.isEmpty())) {
					main_tag.append(current_token);
					continue;
				}
			}

			if(removing_main) {
				if(allowed_in_tag(current_token, !removed_tag.isEmpty())) {
					removed_tag.append(current_token);
					continue;
				}
			}

			if(found_replace) {
				if(allowed_in_tag(current_token, !replaced_tag.isEmpty())) {
					replaced_tag.append(current_token);
					continue;
				}
			}

			if(found_mapped) {
				if (is_negation_token(current_token) && consequent_tag.isEmpty()) {
					consequent_tag.append(current_token);
					continue;
				}

				if(allowed_in_tag(current_token, !consequent_tag.isEmpty())) {
					consequent_tag.append(current_token);
					continue;
				}
			}

			if(is_negation_token(current_token) && main_tag.isEmpty() && regex_source.isEmpty()) {
				appending_main = false;
				found_mapped = false;
				found_replace = false;
				removing_main = true;
				continue;
			}

			if(is_replacement_token(current_token) && !main_tag.isEmpty()) {
				appending_main = false;
				removing_main = false;
				found_mapped = false;
				found_replace = true;
				continue;
			}

			if(is_implication_token(current_token) && !main_tag.isEmpty()) {
				appending_main = false;
				removing_main = false;
				found_replace = false;
				found_mapped = true;
				continue;
			}

			if(current_token == ',') {
				if(removing_main && !removed_tag.isEmpty()) {
					removed_tags_list.push_back(removed_tag);
					removed_tag.clear();
				}

				if(found_replace && !replaced_tag.isEmpty()) {
					replaced_tags_list.push_back(replaced_tag);
					replaced_tag.clear();
				}
				if(found_mapped && !consequent_tag.isEmpty()) {
					consequent_tags_list.push_back(consequent_tag);
					consequent_tag.clear();
				}
			}

		} // for

		if(!removed_tag.isEmpty()) {
			removed_tags_list.push_back(removed_tag);
		}

		if(!replaced_tag.isEmpty()) {
			replaced_tags_list.push_back(replaced_tag);
		}

		if(!consequent_tag.isEmpty()) {
			consequent_tags_list.push_back(consequent_tag);
		}

#ifdef QT_GUI_LIB
		QString color_name;
		QColor tag_color = parse_color(comment, &color_name);
		if (!tag_color.isValid()) {
			// try to find the category in the comment
			const auto split_comment = util::split(comment);
			for (const auto& part : split_comment) {
				// check if this string is a category name
				auto pos = custom_categories.find(part);
				if (pos != custom_categories.end()) {
					Q_ASSERT(pos->second.isValid());
					tag_color = pos->second;
				}
			}
		}
#endif

		if(!main_tag.isEmpty()) {

			// don't add regexes as main tags
			if (main_tag != regex_source) {
				main_tags_list.push_back(main_tag);
				add_tag_kind(main_tag, TagKind::Main);
			}

			if(!comment.isEmpty()) {
				comment = comment.trimmed();

#ifdef QT_GUI_LIB
				if (tag_color.isValid()) {
					m_tag_colors.emplace(main_tag, tag_color);
					comment.remove(color_name);
					comment = comment.trimmed();
				}
#endif

				auto it = m_comment_tooltips.find(main_tag);
				if(it != m_comment_tooltips.end()) {
					it->second.append(QStringLiteral(", "));
					it->second.append(comment);
				} else {
					m_comment_tooltips.emplace(main_tag, comment);
				}
			}
		}

		for(const auto& remtag : qAsConst(removed_tags_list)) {
			m_removed_tags.emplace(remtag);
			add_tag_kind(remtag, TagKind::Removed);
#ifdef QT_GUI_LIB
			// add removed tag color
			if (tag_color.isValid())
				m_tag_colors.emplace(remtag, tag_color);
#endif
		}

		for (const auto& repltag : qAsConst(replaced_tags_list)) {
			m_replaced_tags.emplace(repltag, main_tag);
			add_tag_kind(repltag, TagKind::Replaced);
		}

		for(const auto& cons_tag : qAsConst(consequent_tags_list)) {
			m_related_tags.emplace(main_tag, cons_tag);
			add_tag_kind(cons_tag, TagKind::Consequent);
		}

	}
	return main_tags_list;
}


#ifdef QT_GUI_LIB
QColor TagParser::parse_color(const QString& input, QString * color_name)
{
	QColor tag_color;
	// early out
	if (input.isEmpty())
		return tag_color;

	auto split_str = util::split(input);

	for (const auto& part : qAsConst(split_str)) {
		if (!part.startsWith('#'))
			continue;

		// try hex color
		tag_color.setNamedColor(part);
		if (tag_color.isValid()) {
			tag_color.setAlpha(255);
			if (color_name) *color_name = part;
			break;
		}

		// try named color (without #)
		tag_color.setNamedColor(part.midRef(1));
		if (tag_color.isValid()) {
			tag_color.setAlpha(255);
			if (color_name) *color_name = part;
			break;
		}
	}

	return tag_color;
}


// parse #pragma statement
bool TagParser::parse_pragma(const QString& line, std::unordered_map<QString, QColor>& custom_categories)
{
	if (line.startsWith('#')) {
		const auto pragmas = util::split(line.mid(1));
		if (pragmas.size() > 1 && pragmas[0] == QStringLiteral("pragma")) {

			auto get_custom_color = [](const auto& pragmas, const auto& param_name) {
				QColor res;
				if (pragmas.size() >= 3) {
					if (pragmas[1] == param_name) {
						res = parse_color(pragmas[2]);
					}
				}
				return res;
			};

			auto implication = get_custom_color(pragmas, "implied_color");
			if (implication.isValid()) {
				m_custom_implication_color = implication;
			}

			auto removal = get_custom_color(pragmas, "removed_color");
			if (removal.isValid()) {
				m_custom_removal_color = removal;
			}

			auto replacement = get_custom_color(pragmas, "replaced_color");
			if (replacement.isValid()) {
				m_custom_replacement_color = replacement;
			}

			auto get_category_color = [](const auto& pragmas, QString& category) {
				QColor res;
				if (pragmas.size() >= 4) {
					if (pragmas[1] == "category") {
						res = parse_color(pragmas[3]);
						if (res.isValid()) {
							category = pragmas[2];
						}
					}
				}
				return res;
			};

			QString category_name;
			auto category_color = get_category_color(pragmas, category_name);
			if (category_color.isValid()) {
				custom_categories.emplace(category_name, category_color);
			}
		}
		return true;
	}
	return false;
}
#endif

QStringList TagParser::related_tags(TagEditState& state, const QString &tag) const
{
	QStringList relatives;
	if (state.needAddRelated(tag)) {
		auto related_tags_range = m_related_tags.equal_range(tag);

		/* Mark this tag as processed. This also prevents infinite looping.
		 * If user decides to remove related tag(s), they will not be added again.
		 */
		for(auto it = related_tags_range.first; it != related_tags_range.second; ++it) {
			auto consequent = it->second;
			/* Check if consequent tag was added before as well,
			 * because several tags can imply the same consequent, resulting in a cycle.
			 */
			if (state.needAdd(consequent)) {
				/* Add consequent tag for this tag */
				relatives.push_back(consequent);
			}
		}
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

void TagParser::remove_explicit(TagEditState& state, QStringList & text_list) const
{
	for (auto& tag : text_list) {
		// find "-tag"
		auto neg_pos = std::find_if(text_list.begin(), text_list.end(),
			[&tag](const auto& s) {
				return starts_with_negation_token(s) && s.midRef(1) == tag;
			});
		if (neg_pos != text_list.end()) {
			// mark this tag as processed
			if (state.needRemove(tag)) {
				tag.clear();
			}
			// negated tag will be removed by caller anyway
		}
	}
}


void TagEditState::clear()
{
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


