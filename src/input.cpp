/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "input.h"
#include <algorithm>
#include <QMessageBox>
#include <QFileInfo>
#include <QApplication>

TagInput::TagInput(QWidget *parent) : QLineEdit(parent), m_index(0)
{
	setMinimumHeight(30);

	m_completer = std::make_unique<MultiSelectCompleter>(QStringList(), nullptr);
}

void TagInput::fixTags(bool sort)
{
	QStringList list = text().split(" ", QString::SkipEmptyParts);

	for(int i = 0; i < list.size(); ++i) {
		list[i].toLower();

		remove_if_short(list[i]);
		remove_if_unwanted(list[i]);
		replace_booru_and_id(list[i]);
		list += replacement_tags(list[i]);
		list += related_tags(list[i]);
	}

	list.removeDuplicates();

	if(sort)
		list.sort();

	QString newname;
	for(const auto& tag : list)
		if( !tag.isEmpty() ) {
			newname.append(tag);
			newname.append(' ');
		}

	newname.remove(newname.length()-1, 1); // remove trailing space
	setText(newname);
}

void TagInput::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_Tab) {
		if((!next_completer())&&(!next_completer()))
			return;
		setText(m_completer->currentCompletion());
		return;
	}

	if(event->key() == Qt::Key_Escape) {
		clearFocus();
		return;
	}

	if(event->key() == Qt::Key_Enter || event->key()== Qt::Key_Return)
	{
		fixTags();
		clearFocus();
		return;
	}

	if(event->key() == Qt::Key_Space) {
		/* Don't sort tags, haven't finished editing yet */
		fixTags(false);
	}

	QLineEdit::keyPressEvent(event);
	m_completer->setCompletionPrefix(text());
	m_index = 0;
}

void TagInput::loadTagFile(const QString& file)
{
	QFileInfo fi(file);
	QFile f;
	if(!fi.exists()) {
		if(fi.isRelative()) {
			f.setFileName(qApp->applicationDirPath() + "/" + file);
		}
	} else {
		f.setFileName(file);
	}

	if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QMessageBox::warning(this,
			tr("Error opening tag file"),
			tr("<p>Could not open <b>%1</b>.</p><p>Tag autocomplete will be disabled.</p>").arg(file));
		return;
	}

	m_related_tags.clear();
	m_replaced_tags.clear();
	m_removed_tags.clear();

	QTextStream in(&f);
	in.setCodec("UTF-8");

	m_completer = std::make_unique<MultiSelectCompleter>(parse_tags_file(in), nullptr);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(m_completer.get());
}

void TagInput::reloadAdditionalTags()
{
	/* Remove elements with value == key to restore original state */
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
	for(auto it = m_removed_tags.begin(); it != m_removed_tags.end(); ++it) {
		if (it->second == true) {
			it->second = false;
		}
	}
}

//------------------------------------------------------------------------------

QStringList TagInput::parse_tags_file(QTextStream &input)
{
	QString current_line, main_tag, removed_tag, replaced_tag, mapped_tag;
	QStringList main_tags_list, removed_tags_list, replaced_tags_list, mapped_tags_list;

	auto allowed_in_file = [](QChar c){
		return	c.isLetterOrNumber() ||
			c == '_' ||
			c == '-' ||
			c == ':' ||
			c == ';' ||
			c == ',' ||
			c == '=' ||
			c == ' ' ||
			c == '\t';
	};

	auto allowed_in_tag = [](QChar c){
		return c.isLetterOrNumber() || c == '_' || c == ';';
	};

	while(!input.atEnd()) {
		current_line = input.readLine();

		main_tag.clear();
		removed_tag.clear();
		replaced_tag.clear();
		mapped_tag.clear();

		removed_tags_list.clear();
		replaced_tags_list.clear();
		mapped_tags_list.clear();

		bool appending_main = true, removing_main = false, found_replace = false, found_mapped = false;

		for(int i = 0; i < current_line.length(); ++i) {
			if(!allowed_in_file(current_line[i])) {
				break; // go to next line
			}

			if(appending_main) {
				if(allowed_in_tag(current_line[i])) {
					main_tag.append(current_line[i]);
					continue;
				}
			}

			if(removing_main) {
				if(allowed_in_tag(current_line[i])) {
					removed_tag.append(current_line[i]);
					continue;
				}
			}

			if(found_replace) {
				if(allowed_in_tag(current_line[i])) {
					replaced_tag.append(current_line[i]);
					continue;
				}
			}

			if(found_mapped) {
				if(allowed_in_tag(current_line[i])) {
					mapped_tag.append(current_line[i]);
					continue;
				}
			}

			if(current_line[i] == '-' && main_tag.isEmpty()) {
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
		}

		for(auto && remtag : removed_tags_list) {
			m_removed_tags.insert(std::pair<QString, bool> (remtag, false));
		}

		for (auto && repltag : replaced_tags_list) {
			m_replaced_tags.insert(std::pair<QString,QString>(repltag, main_tag));
		}

		for(auto && maptag : mapped_tags_list) {
			m_related_tags.insert(std::pair<QString,QString>(main_tag,maptag));
		}
	}
	return main_tags_list;
}

void TagInput::remove_if_short(QString& tag)
{
	if(tag.length() == 1) {
		/* Allow single digits */
		if(!tag[0].isDigit())
			tag.clear();
	}
}

using qs_ummap = std::unordered_multimap<QString,QString>;

QStringList TagInput::related_tags(const QString &tag)
{
	QStringList ret;
	auto mapped_range = m_related_tags.equal_range(tag);
	auto key_eq_val = std::find_if(mapped_range.first, mapped_range.second, [](qs_ummap::value_type p) { return p.first == p.second; });
	/* Check if tag hasn't been added already */
	if((mapped_range.first != m_related_tags.end()) && (key_eq_val == mapped_range.second)) {
		/* Add all related tags for this tag */
		std::for_each( mapped_range.first, mapped_range.second,
			[&](qs_ummap::value_type p) {
				ret.push_back(p.second);
			});
		/* Mark this tag as processed by inserting additional element into hashmap,
		 * whose key and value are both equal to this tag */
		m_related_tags.insert(std::pair<QString,QString>(tag,tag));
	}
	return ret;
}

QStringList TagInput::replacement_tags(QString &tag)
{
	QStringList ret;
	auto replaced_range = m_replaced_tags.equal_range(tag);
	auto key_eq_val = std::find_if(replaced_range.first, replaced_range.second, [](qs_ummap::value_type p) { return p.first == p.second; });
	/* Check if tag hasn't been added already */
	if((replaced_range.first != m_replaced_tags.end()) && (key_eq_val == replaced_range.second)) {
		/* Add all replacement tags for this tag and remove it */
		std::for_each( replaced_range.first, replaced_range.second,
			[&](qs_ummap::value_type p) {
				ret.push_back(p.second);
			});
			/* Mark this tag as processed by inserting additional element into hashmap,
			 * whose key and value are both equal to this tag */
			m_replaced_tags.insert(std::pair<QString,QString>(tag,tag));
			tag.clear();
	}
	return ret;
}

void TagInput::remove_if_unwanted(QString &tag)
{
	auto removed_pos = m_removed_tags.find(tag);
	/* Check if it hasn't been removed already */
	if(removed_pos != m_removed_tags.end()) {
		if(removed_pos->second == false) {
			tag.clear();
			/* Mark this tag as processed */
			removed_pos->second = true;
		}
	}
}

void TagInput::replace_booru_and_id(QString &tag)
{
	const char* boorus[]  = {"yande.re", "Konachan.com"};
	const char* replacement[] = { "yandere_", "konachan_"};
	constexpr int len = sizeof(boorus) / sizeof(*boorus);

	static bool found_booru[len] = {0};
	static int id;

	for(int i = 0; i < len; ++i) {
		if(tag == boorus[i]) {
			found_booru[i] = true;
			tag.clear();
			break;
		}
	}

	if((id = tag.toInt()) != 0) {
		for(int i = 0; i < len; ++i) {
			if(found_booru[i]) {
				tag = QString(replacement[i]) + QString::number(id);
			}
			found_booru[i] = false;
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
