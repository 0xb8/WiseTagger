/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "input.h"
#include "global_enums.h"
#include "util/imageboard.h"
#include "util/misc.h"
#include <algorithm>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSettings>
#include <QTextCodec>
#include <QTextStream>

namespace logging_category {
	Q_LOGGING_CATEGORY(input, "TagInput")
}
#define pdbg qCDebug(logging_category::input)
#define pwarn qCWarning(logging_category::input)

TagInput::TagInput(QWidget *_parent) : QLineEdit(_parent), m_index(0)
{
	updateSettings();
	m_completer = std::make_unique<MultiSelectCompleter>(QStringList(), nullptr);
}

void TagInput::fixTags(bool sort)
{
	for (auto& p : qAsConst(m_regexps)) {

		auto& re = p.first;
		auto& src = p.second;

		QRegularExpressionMatch m = re.match(text());
		if (m.hasMatch()) {

			auto capture_start = m.capturedStart();
			auto capture_end = m.capturedEnd();

			auto t = text();
			t.remove(capture_start, capture_end - capture_start);

			for (auto r : related_tags(src)) {
				for (auto capture_group : re.namedCaptureGroups()) {
					if (!capture_group.isEmpty()) {
						r.replace(capture_group, m.captured(capture_group));
					}
				}
				t.append(' ');
				t.append(r);
			}
			if (!t.isEmpty()) {
				QLineEdit::setText(t);
			}
		}
	}


	m_text_list = text().split(QChar(' '), QString::SkipEmptyParts);
	QSettings settings;
	if(settings.value(QStringLiteral("imageboard/replace-tags"), false).toBool())
		ib::replace_imageboard_tags(m_text_list);

	if(settings.value(QStringLiteral("imageboard/restore-tags"), true).toBool())
		ib::restore_imageboard_tags(m_text_list);

	for(int i = 0; i < m_text_list.size(); ++i) {
		remove_if_unwanted(m_text_list[i]); // TODO: refactor using iterators
		m_text_list += replacement_tags(m_text_list[i]);
		m_text_list += related_tags(m_text_list[i]);
	}

	m_text_list.removeDuplicates();
	m_text_list.erase(
		std::remove_if(
			m_text_list.begin(),
			m_text_list.end(),
			[](const auto& s){ return s.isEmpty(); }),
		m_text_list.end());

	tag_iterator tag, id;
	if(!ib::find_imageboard_tags(m_text_list.begin(), m_text_list.end(),tag,id)) {
		id = m_text_list.begin();
	} else {
		// sorting should begin from next tag
		std::advance(id, 1);
	}

	if(sort) {
		std::sort(id, m_text_list.end());
	}

	// if the author tag is present and should be forced to be first, move it first
	if(settings.value(QStringLiteral("imageboard/force-author-first"), false).toBool()) {
		// due to C++ literal strings rules all backslashes inside the pattern string needs escaping with another backslash
		// the pattern matches to (not-a-space-tab-newline)@AA - (not-a-space-tab-newline)@ZZ;
		// the list is then prepended by those having ↑ and original entries of ↑ are removed
		m_text_list = m_text_list.filter(QRegularExpression(R"~(\[?[^ \t\n\r]+@[A-Z]{2}\]?)~")) + m_text_list;
		m_text_list.removeDuplicates();
	}

	auto newname = m_text_list.join(' ');
	util::replace_special(newname);
	updateText(newname);
}

void TagInput::setTags(const QString& s)
{
	auto text_list = text().split(QChar(' '), QString::SkipEmptyParts);

	tag_iterator tag, id;
	if(ib::find_imageboard_tags(text_list.begin(), text_list.end(),tag,id)) {
		id = std::next(id);
	}

	QString res;
	for (auto it = tag; it != id; ++it) {
		res.append(*it);
		res.append(' ');
	}

	res.append(s);
	setText(res);
}

QString TagInput::tags() const
{
	auto text_list = text().split(QChar(' '), QString::SkipEmptyParts);
	tag_iterator tag, id;
	if(ib::find_imageboard_tags(text_list.begin(), text_list.end(),tag,id)) {
		id = std::next(id);
	} else {
		id = text_list.begin();
	}

	QString res;
	for (auto it = id; it != text_list.end(); ++it) {
		res.append(*it);
		res.append(QChar(' '));
	}
	res.remove(res.length()-1, 1); // remove trailing space
	return res;
}

void TagInput::keyPressEvent(QKeyEvent *m_event)
{
	if(m_event->key() == Qt::Key_Tab) {
		if((!next_completer())&&(!next_completer()))
			return;
		updateText(m_completer->currentCompletion());
		return;
	}

	if(m_event->key() == Qt::Key_Escape) {
		clearFocus();
		return;
	}

	if(m_event->key() == Qt::Key_Enter || m_event->key()== Qt::Key_Return)
	{
		fixTags();
		clearFocus();
		return;
	}
	// NOTE: Shift+Space - workaround for space not working as expected when typing in the middle of the word.
	if(m_event->key() == Qt::Key_Space && !(m_event->modifiers() & Qt::ShiftModifier)) {
		/* Don't sort tags, haven't finished editing yet */
		fixTags(false);
	}

	QLineEdit::keyPressEvent(m_event);
	m_completer->setCompletionPrefix(text());
	m_index = 0;
}

void TagInput::focusInEvent(QFocusEvent * event)
{
	QLineEdit::focusInEvent(event);
	QSettings settings;
	bool space = event->isAccepted() && event->gotFocus() && event->reason() == Qt::TabFocusReason;
	if (space && settings.value("window/focus-append", true).toBool()) {
		// add a space to text and move fursor forward
		auto tmp_text = text().trimmed();
		if (!tmp_text.isEmpty())
			tmp_text.append(' ');

		updateText(tmp_text);
		cursorForward(false);
	}
}

static void update_model(QStandardItemModel& model, const QStringList& tags, const std::unordered_map<QString,QString>& comments)
{
	model.clear();
	model.setColumnCount(1);
	model.setRowCount(tags.size());
	for(int i = 0; i < model.rowCount(); ++i) {
		auto index = model.index(i, 0);
		const auto& tag = tags[i];
		if(Q_UNLIKELY(!model.setData(index, tag, Qt::UserRole))) {
			pwarn << "could not set completion data for "<< tag << "at" << index;
		}
		auto it = comments.find(tag);
		if(Q_UNLIKELY(it != comments.end())) {
			//pdbg << "setting comment data for " << tags[i] << ":" << it->second;
			QString data = tag;
			data.append(QStringLiteral("  ("));
			data.append(it->second);
			data.append(QStringLiteral(")"));
			if(Q_UNLIKELY(!model.setData(index, data, Qt::DisplayRole)))
				pwarn << "could not set tag comment:" << data;
		} else {
			model.setData(index, tag, Qt::DisplayRole);
		}
	}
}

void TagInput::loadTagData(const QByteArray& data)
{
	if(data.isEmpty()) {
		return;
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

	update_model(m_tags_model, m_tags_from_file, m_comment_tooltips);

	m_completer = std::make_unique<MultiSelectCompleter>(&m_tags_model, nullptr);
	m_completer->setCompletionRole(Qt::UserRole);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(m_completer.get());
}

void TagInput::setText(const QString &s)
{
	clearTagState();
	updateText(s);
	m_initial_text = s;
}

void TagInput::clearTagState()
{
	/* Remove elements where (value == key) to restore original state */
	for(auto it = m_related_tags.begin(); it != m_related_tags.end(); ) {
		if (it->second == it->first) {
			it = m_related_tags.erase(it);
		} else ++it;
	}

	for(auto it = m_replaced_tags.begin(); it != m_replaced_tags.end(); ) {
		if (it->second == it->first) {
			it = m_replaced_tags.erase(it);
		} else ++it;
	}

	/* Restore state */
	for(auto& pair : m_removed_tags) {
		pair.second = false;
	}
}

void TagInput::updateText(const QString &t)
{
	m_text_list = t.split(QChar(' '), QString::SkipEmptyParts);
	QLineEdit::setText(t);
}

QString TagInput::postURL() const
{
	return ib::get_imageboard_meta(m_text_list.begin(), m_text_list.end()).post_url;
}

QString TagInput::postTagsApiURL() const
{
	return  ib::get_imageboard_meta(m_text_list.begin(), m_text_list.end()).post_api_url;
}

bool TagInput::hasTagFile() const
{
	return !m_tags_from_file.isEmpty();
}

QStringList TagInput::getAddedTags(bool exclude_tags_from_file) const
{
	auto initial_tags = m_initial_text.split(QChar(' '), QString::SkipEmptyParts);
	auto new_tags = text().split(QChar(' '), QString::SkipEmptyParts);
	initial_tags.sort();
	new_tags.sort();
	QStringList ret;
	std::set_difference(new_tags.begin(), new_tags.end(), initial_tags.begin(), initial_tags.end(), std::back_inserter(ret));

	if(exclude_tags_from_file) {
		auto tags_file = m_tags_from_file;
		tags_file.sort();
		QStringList ret2;
		std::set_difference(ret.begin(), ret.end(), tags_file.begin(), tags_file.end(), std::back_inserter(ret2));
		return ret2;
	}
	return ret;
}

void TagInput::updateSettings()
{
	QSettings s;
	QFont font(s.value(QStringLiteral("window/font"), QStringLiteral("Consolas")).toString());
	auto view_mode = s.value(QStringLiteral("window/view-mode")).value<ViewMode>();
	if(view_mode == ViewMode::Normal) {
		font.setPixelSize(s.value(QStringLiteral("window/font-size"), 14).toInt());
		setMinimumHeight(m_minimum_height);
		setFrame(true);
		setStyleSheet(QStringLiteral(""));
	}
	if(view_mode == ViewMode::Minimal) {
		font.setPixelSize(s.value(QStringLiteral("window/font-size-minmode"), 12).toInt());
		setMinimumHeight(m_minimum_height_minmode);
		setFrame(false);
		setStyleSheet(QStringLiteral("border-top-width: 1px; border-top-style: outset;"));
	}
	setFont(font);
}
//------------------------------------------------------------------------------

QStringList TagInput::parse_tags_file(QTextStream *input)
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
		if (current_line.front() == '\'') {

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
			m_removed_tags.insert(std::pair<QString, bool> (remtag, false));
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

QStringList TagInput::related_tags(const QString &tag)
{
	QStringList relatives;
	auto related_tags_range = m_related_tags.equal_range(tag);
	auto key_eq_val = std::find_if(related_tags_range.first,
				       related_tags_range.second,
				       [](const auto& p) { return p.first == p.second; });
	bool found_tag_entry = related_tags_range.first != m_related_tags.end();
	bool tag_already_processed = key_eq_val != related_tags_range.second;

	if(found_tag_entry && !tag_already_processed) {
		std::for_each(related_tags_range.first, related_tags_range.second,
			[&relatives](const auto& p)
			{	/* Add all related tags for this tag */
				relatives.push_back(p.second);
			});

		/* Mark this tag as processed by inserting marker element into hashmap,
		   whose key and value are both equal to this tag. This also prevents
		   infinite looping.
		   If user decides to remove related tag(s), they will not be added again.
		 */
		m_related_tags.insert(std::pair<QString,QString>(tag,tag));
	}
	return relatives;
}

QStringList TagInput::replacement_tags(QString &tag)
{
	QStringList replacements;
	auto replaced_tags_range = m_replaced_tags.equal_range(tag);
	auto key_eq_val = std::find_if(replaced_tags_range.first,
				       replaced_tags_range.second,
				       [](const auto& p) { return p.first == p.second; });
	bool found_tag_entry = replaced_tags_range.first != m_replaced_tags.end();
	bool tag_already_processed = key_eq_val != replaced_tags_range.second;


	if(found_tag_entry && !tag_already_processed) {
		std::for_each(replaced_tags_range.first, replaced_tags_range.second,
			[&replacements](const auto& p)
			{	/* Add all replacement tags for this tag */
				replacements.push_back(p.second);
			});

		/* Mark this tag as processed by inserting marker element into hashmap,
		   whose key and value are both equal to this tag. This also prevents
		   infinite looping.
		   If user decides to remove replaced tag(s) and enters original one(s),
		   they will not be replaced again.
		 */
		m_replaced_tags.insert(std::pair<QString,QString>(tag,tag));
		tag.clear(); /* Clear original tag */
	}
	return replacements;
}

void TagInput::remove_if_unwanted(QString &tag)
{
	auto removed_tag = m_removed_tags.find(tag);
	if(removed_tag != m_removed_tags.end()) {
		/* Allow user to enter and keep tag which has been autoremoved */
		if(removed_tag->second == false) {
			tag.clear();
			/* Next time this tag will not be removed */
			removed_tag->second = true;
		}
	}
}

bool TagInput::next_completer()
{
    if(m_completer->setCurrentRow(m_index++))
	    return true;
    m_index = 0;
    return false;
}
