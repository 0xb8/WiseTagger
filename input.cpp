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

#include <QTextStream>
#include <QTextCodec>

TagInput::TagInput(QWidget *parent) : QLineEdit(parent), index(0)
{
	setMinimumHeight(30);
	completer = std::unique_ptr<MultiSelectCompleter>(new MultiSelectCompleter(QStringList(),this));

	QFont font("Consolas");
	font.setStyleHint(QFont::TypeWriter);
	font.setPixelSize(14);
	setFont(font);
}

void TagInput::fixTags(bool nosort)
{
	bool yandere = false;
	int id = 0;
	QStringList list = text().split(" ", QString::SkipEmptyParts);
	QStringList added;
	for(auto && tag : list) {
		tag.toLower();
		if(tag == "yande.re") {
			yandere = true;
		}
		if(id == 0) {
			id = tag.toInt();
		}

		auto range = mapped_tags.equal_range(tag);
		auto val_equals_key = std::find_if(range.first,range.second,[&](QStringUnMap::value_type p){
				return p.first == p.second;
		});

		/* We don't want to push additional tags every time,
		 * so after putting them first time, we add a new
		 * element to mapped_tags hashtable with key == value,
		 * and check if that element exists before pushing additional tags
		 * Note that such elements will be deleted from mapped tags when andother file is loaded, or tag file gets reloaded.
		 */
		if(val_equals_key == range.second) { // if not found
			std::for_each( range.first, range.second, [&](QStringUnMap::value_type p) {
				added.push_back(p.second);
			});
			if(range.first != mapped_tags.end()) { // if we actually have added something
				// insert element with its key == value
				mapped_tags.insert(std::pair<QString,QString>(tag,tag));
			}
		}
	}

	list += added;

	if(yandere) {
		list.removeAll("yande.re");
		if(id != 0) {
			list.removeAll(QString::number(id));
			list.push_back(QString("yandere_") + QString::number(id));
		} else {
			list.push_back("yandere(booru)");
		}
	}

	if(!nosort) {
		list.sort(Qt::CaseInsensitive);
	}

	list.removeDuplicates();

	QString newname;
	for(const auto& tag : list) {
		if( !tag.isEmpty() ) {
			newname.append(tag);
			newname.append(' ');
		}
	}
	newname.remove(newname.length()-1, 1); // remove trailing space
	setText(newname);
}

bool TagInput::nextCompleter()
{
    if(completer->setCurrentRow(index++))
	    return true;
    index = 0;
    return false;
}

void TagInput::keyPressEvent(QKeyEvent *event)
{

	if(event->key() == Qt::Key_Tab) {
		if((!nextCompleter())&&(!nextCompleter()))
			return;
		setText(completer->currentCompletion());
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
		fixTags(true);
	}

	QLineEdit::keyPressEvent(event);
	completer->setCompletionPrefix(text());
	index = 0;
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
		QMessageBox::warning(NULL, tr("Error opening tag file"), tr("Could not open %1. Tag autocomplete will be disabled.").arg(file));
		return;
	}

	mapped_tags.clear();
	QTextStream in(&f);
	in.setCodec("UTF-8");
	QString line, main_tag, map_tag;
	QStringList main_tags, map_tags;
	int main_tag_length;
	int line_number = 1;
	auto charAllowed = [](QChar c){
		return c.isLetterOrNumber() || c == '_' || c == ':' || c == ';' || c == ',' || c == ' ' || c == '\t';
	};
	auto charMainTag = [](QChar c){
		return c.isLetterOrNumber() || c == '_' || c == ';';
	};

	while(!in.atEnd()) {
		line = in.readLine();
		main_tag.clear();
		map_tag.clear();
		map_tags.clear();
		main_tag_length = 0;

		for(int i = 0; i < line.length(); ++i) {
			if(!charAllowed(line[i])) {
			#ifndef NO_PARSER_DEBUG
				qDebug("Warning: invalid character <%c> (%s:%d:%d)\n  %s", line[i].unicode(), qPrintable(QFileInfo(file).fileName()), line_number, i+1, qPrintable(line));
				std::string err("  ");
				for(int j = 0; j < i; ++j) {
					if(line[j] == '\t') {
						err.append("\t");
					} else {
						err.append(" ");
					}
				}
				err.append("^----");
				qDebug("%s",err.c_str());
			#endif
				break;
			}

			if(main_tag_length == 0) {
				if(charMainTag(line[i])) {
					main_tag.append(line[i]);
					continue;
				}
				if(line[i] == ':' && !main_tag.isEmpty()) {
					main_tag_length = main_tag.length();
				#ifndef NO_PARSER_DEBUG
					qDebug("colon delimiter @ %s (line %d:%d); resulting main tag is [%s]", qPrintable(line), line_number, i+1, qPrintable(main_tag));
				#endif
					continue;
				}
			} else {
				if(charMainTag(line[i])) {
					map_tag.append(line[i]);
					continue;
				}
				if(line[i] == ',' && !map_tag.isEmpty()) {
					map_tags.push_back(map_tag);
				#ifndef NO_PARSER_DEBUG
					qDebug("comma delimiter @ %s (line %d:%d); resulting mapped tag is [%s]", qPrintable(line), line_number, i+1, qPrintable(map_tag));
					qDebug("pushing mapped tag (@ %s) [%s] (line %d)", qPrintable(main_tag), qPrintable(map_tag), line_number);
				#endif
					map_tag.clear();
				}
			}
		} // for

		if(!main_tag.isEmpty()) {
		#ifndef NO_PARSER_DEBUG
			qDebug("pushing main tag [%s] (line %d)", qPrintable(main_tag), line_number);
		#endif
			main_tags.push_back(main_tag);
		}
		if(!map_tag.isEmpty()) {
			map_tags.push_back(map_tag);
		#ifndef NO_PARSER_DEBUG
			qDebug("pushing mapped tag (@ %s) [%s] (line %d)", qPrintable(main_tag), qPrintable(map_tag), line_number);
		#endif
		}

		for(auto & maptag : map_tags) {
			mapped_tags.insert(std::pair<QString,QString>(main_tag,maptag));
		}
		++line_number;
	}

	completer = std::unique_ptr<MultiSelectCompleter>(new MultiSelectCompleter(main_tags,this));
	completer->setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(completer.get());
}

void TagInput::reloadMappedTags()
{
	for(auto it = mapped_tags.begin(); it != mapped_tags.end(); ) {
		if (it->second == it->first) {
			it = mapped_tags.erase(it);
		} else ++it;
	}
}
