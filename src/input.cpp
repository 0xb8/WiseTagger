/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "input.h"
#include "global_enums.h"
#include "util/misc.h"

#include <QKeyEvent>
#include <QLoggingCategory>
#include <QSettings>


namespace logging_category {
	Q_LOGGING_CATEGORY(input, "TagInput")
}
#define pdbg qCDebug(logging_category::input)
#define pwarn qCWarning(logging_category::input)

#include "util/imageboard.h"

TagInput::TagInput(QWidget *_parent) : QLineEdit(_parent), m_index(0)
{
	updateSettings();
	m_completer = std::make_unique<MultiSelectCompleter>(QStringList(), nullptr);
	connect(&m_tag_parser, &TagParser::parseError, this, &TagInput::parseError);
}

void TagInput::fixTags(bool sort)
{
	QSettings s;

	TagParser::FixOptions opts;
	opts.replace_imageboard_tags = s.value(QStringLiteral("imageboard/replace-tags"), false).toBool();
	opts.restore_imageboard_tags = s.value(QStringLiteral("imageboard/restore-tags"), true).toBool();
	opts.force_author_handle_first = s.value(QStringLiteral("imageboard/force-author-first"), false).toBool();
	opts.sort = sort;
	m_text_list = m_tag_parser.fixTags(m_edit_state, text(), opts);

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
		releaseKeyboard();
		parentWidget()->setFocus();
		return;
	}

	if(m_event->key() == Qt::Key_Enter || m_event->key()== Qt::Key_Return)
	{
		fixTags();
		releaseKeyboard();
		parentWidget()->setFocus();
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

static void update_model(QStandardItemModel& model, const TagParser& parser)
{
	const auto& tags = parser.getAllTags();
	model.clear();
	model.setColumnCount(1);
	model.setRowCount(tags.size());
	for(int i = 0; i < model.rowCount(); ++i) {
		auto index = model.index(i, 0);
		const auto& tag = tags[i];
		if(Q_UNLIKELY(!model.setData(index, tag, Qt::UserRole))) {
			pwarn << "could not set completion data for "<< tag << "at" << index;
		}

		auto comment = parser.getComment(tag);
		if(Q_UNLIKELY(!comment.isEmpty())) {
			QString data = tag;
			data.append(QStringLiteral("  ("));
			data.append(comment);
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
	if (!m_tag_parser.loadTagData(data))
		return;

	update_model(m_tags_model, m_tag_parser);

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
	m_edit_state.clear();
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
	return m_tag_parser.hasTagFile();
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
		auto tags_file = m_tag_parser.getAllTags();
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

bool TagInput::next_completer()
{
    if(m_completer->setCurrentRow(m_index++))
	    return true;
    m_index = 0;
    return false;
}
