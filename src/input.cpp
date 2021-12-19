/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "input.h"
#include "global_enums.h"
#include "util/strings.h"

#include <vector>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QSettings>
#include <QTextLayout>
#include <QCoreApplication>


namespace logging_category {
	Q_LOGGING_CATEGORY(input, "TagInput")
}
#define pdbg qCDebug(logging_category::input)
#define pwarn qCWarning(logging_category::input)

#include "util/imageboard.h"

TagInput::TagInput(QWidget *_parent) : QLineEdit(_parent), m_index(0)
{
	m_completer = std::make_unique<MultiSelectCompleter>(QStringList(), nullptr);
	connect(&m_tag_parser, &TagParser::parseError, this, &TagInput::parseError);
	connect(this, &QLineEdit::textEdited, this, [this](auto){
		const auto tag_list = tags_list();
		classifyText(tag_list);
	});
}

void TagInput::fixTags(bool sort)
{
	auto opts = TagParser::FixOptions::from_settings();
	opts.sort = sort;
	m_text_list = m_tag_parser.fixTags(m_edit_state, text(), opts);

	auto newname = util::join(m_text_list);
	util::replace_special(newname);
	updateText(newname);
}

void TagInput::setTags(const QString& s)
{
	auto text_list = util::split(text());
	QStringList imageboard_ids;

	tag_iterator tag_begin = text_list.begin(), id = tag_begin;

	// find long imageboard id tag
	if (ib::find_imageboard_tags(tag_begin, text_list.end(), tag_begin, id)) {
		id = std::next(id);
		for (auto it = tag_begin; it != id; ++it) {
			imageboard_ids.append(*it);
		}
	}

	// find all short imageboard id tags
	tag_begin = text_list.begin();
	while (ib::find_short_imageboard_tag(tag_begin, text_list.end(), tag_begin)) {
		imageboard_ids.append(*tag_begin);
		tag_begin = std::next(tag_begin);
	}

	QString res = util::join(imageboard_ids);
	res.append(' ');
	res.append(s);
	setText(res);
}

QString TagInput::tags() const
{
	return util::join(tags_list());
}

QStringList TagInput::tags_list() const
{
	auto text_list = util::split(text());

	tag_iterator tag_begin = text_list.begin(), id = tag_begin;

	// clear long imageboard id tag
	if (ib::find_imageboard_tags(tag_begin, text_list.end(), tag_begin, id)) {
		for (auto it = tag_begin; it != std::next(id); ++it)
			it->clear();
	}

	// clear short imageboard id tags
	tag_begin = text_list.begin();
	while (ib::find_short_imageboard_tag(tag_begin, text_list.end(), tag_begin)) {
		tag_begin->clear();
		tag_begin = std::next(tag_begin);
	}

	// remove empty strings from list
	text_list.erase(std::remove_if(text_list.begin(), text_list.end(), [](const auto& str){
		return str.isEmpty();
	}), text_list.end());

	return text_list;
}

const TagParser & TagInput::tag_parser() const
{
	return m_tag_parser;
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

static int update_model(QStandardItemModel& model, const TagParser& parser)
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
				pwarn << "could not set tag display role:" << data;

			if (Q_UNLIKELY((!model.setData(index, comment, Qt::UserRole + 1))))
				pwarn << "could not set tag comment:" << comment;
		} else {
			model.setData(index, tag, Qt::DisplayRole);
		}

		auto color = parser.getColor(tag);
		if (Q_UNLIKELY(color.isValid())) {
			if(Q_UNLIKELY(!model.setData(index, color, Qt::ForegroundRole)))
				pwarn << "could not set tag color role:" << tag << color;
		}
	}
	return model.rowCount();
}

void TagInput::loadTagData(const QByteArray& data)
{
	if (!m_tag_parser.loadTagData(data)) {
		m_tags_model.clear();
		return;
	}

	m_tags_model_num_main_tags = update_model(m_tags_model, m_tag_parser);

	m_completer = std::make_unique<MultiSelectCompleter>(&m_tags_model, nullptr);
	m_completer->setCompletionRole(Qt::UserRole);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(m_completer.get());
	updateText(text());
}

static void set_line_edit_text_formats(QLineEdit& input, const std::vector<QTextLayout::FormatRange>& formats)
{
	QList<QInputMethodEvent::Attribute> attributes;
	for(const auto& fr : formats) {
		QInputMethodEvent::AttributeType type = QInputMethodEvent::TextFormat;
		int start = fr.start - input.cursorPosition();
		int length = fr.length;
		QVariant value = fr.format;
		attributes.append(QInputMethodEvent::Attribute(type, start, length, value));
	}
	QInputMethodEvent event(QString(), attributes);
	QCoreApplication::sendEvent(&input, &event);
}

static void clear_line_edit_text_formats(QLineEdit& input)
{
	set_line_edit_text_formats(input, std::vector<QTextLayout::FormatRange>{});
}

void TagInput::clearTagData()
{
	loadTagData(QByteArray{});
	clear_line_edit_text_formats(*this);
}

void TagInput::setText(const QString &s)
{
	clearTagEditState();
	updateText(s);
	m_initial_text = s;
}

void TagInput::clearTagEditState()
{
	m_edit_state.clear();
}

void TagInput::updateText(const QString &t)
{
	m_text_list = util::split(t);
	QLineEdit::setText(t);

	const auto tag_list = tags_list();
	classifyText(tag_list);
	updateModelRemovedTags(tag_list);
}

void TagInput::classifyText(const QStringList& tag_list)
{

	const auto text = QLineEdit::text();

	std::unordered_set<QStringView> antecedent_tags;
	std::unordered_set<QString> consequent_tags;

	for (const auto& tag: tag_list) {
		if (m_tag_parser.getConsequents(tag, consequent_tags)) {
			antecedent_tags.insert(tag); // add antecedent tag too
		}
	}

	std::vector<QTextLayout::FormatRange> formats;

	const auto consequent_color = m_tag_parser.getCustomKindColor(TagParser::TagKind::Consequent);
	auto replaced_color = m_tag_parser.getCustomKindColor(TagParser::TagKind::Replaced);
	if (!replaced_color.isValid()) {
		replaced_color = getReplacedTagColor();
	}

	auto removed_color = m_tag_parser.getCustomKindColor(TagParser::TagKind::Removed);
	if (!removed_color.isValid()) {
		removed_color = getRemovedTagColor();
	}


	int offset = 0;
	for (const auto& tag : tag_list) {
		offset = text.indexOf(tag, offset);
		if (offset < 0)
			break;

		QTextCharFormat f;
		auto tag_kind = m_tag_parser.classify(tag, tag_list);

		bool has_custom_color = false;

		if (tag_kind & TagParser::TagKind::Unknown) {
			f.setForeground(getUnknownTagColor());
		} else {

			// tag has custom color
			auto color = m_tag_parser.getColor(tag);
			if (Q_UNLIKELY(color.isValid())) {
				has_custom_color = true;
				f.setForeground(color);
			}

			// this tag implies another tag
			if (antecedent_tags.find(tag) != antecedent_tags.end()) {
				f.setFontItalic(true);
				f.setFontWeight(QFont::Bold);
			}
		}

		if (tag_kind & TagParser::TagKind::Consequent) {
			// this tag is implied by other tag
			if (consequent_tags.find(tag) != consequent_tags.end()) {
				f.setFontItalic(true);
				if (consequent_color.isValid() && !has_custom_color) {
					f.setForeground(consequent_color);
				}
			}
		}

		if (tag_kind & TagParser::TagKind::Replaced) {
			f.setForeground(replaced_color);

		}
		if (tag_kind & TagParser::TagKind::Removed) {
			f.setForeground(removed_color);
		}

		QTextLayout::FormatRange fr;
		fr.start = offset;
		fr.length = tag.length();
		fr.format = f;

		formats.push_back(fr);
		offset += tag.length();
	}

	set_line_edit_text_formats(*this, formats);
}

void TagInput::updateModelRemovedTags(const QStringList& tag_list)
{
	int current_row = m_tags_model_num_main_tags;

	// add all tags to model with negation
	for (const auto& tag : tag_list) {

		if (tag.startsWith('-'))
			continue; // shouldn't happen, but just in case

		if (current_row >= m_tags_model.rowCount()) {
			m_tags_model.setRowCount(current_row + 1);
		}

		auto dst_index = m_tags_model.index(current_row, 0);
		const auto negtag = "-" + tag;
		m_tags_model.setData(dst_index, negtag, Qt::DisplayRole);
		m_tags_model.setData(dst_index, negtag, Qt::UserRole);
		++current_row;
	}

	// remove any remaining rows
	m_tags_model.setRowCount(current_row);
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
	auto initial_tags = util::split(m_initial_text);
	auto new_tags = util::split(text());

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

void TagInput::setViewMode(ViewMode view_mode)
{
	QSettings s;
	QFont font(s.value(QStringLiteral("window/font"), QStringLiteral("Consolas")).toString());
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

QColor TagInput::getRemovedTagColor() const
{
	return m_removed_tag_color;
}

void TagInput::setRemovedTagColor(QColor color)
{
	m_removed_tag_color = color;
}

QColor TagInput::getReplacedTagColor() const
{
	return m_replaced_tag_color;
}

void TagInput::setReplacedTagColor(QColor color)
{
	m_replaced_tag_color = color;
}

QColor TagInput::getUnknownTagColor() const
{
	return m_unknown_tag_color;
}

void TagInput::setUnknownTagColor(QColor color)
{
	m_unknown_tag_color = color;
}

QAbstractItemModel * TagInput::completionModel()
{
	return &m_tags_model;
}

bool TagInput::next_completer()
{
    if(m_completer->setCurrentRow(m_index++))
	    return true;
    m_index = 0;
    return false;
}
