/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGINPUT_H
#define TAGINPUT_H

#include "multicompleter.h"
#include "unordered_map_qt.h"
#include <QStringList>
#include <QTextStream>
#include <QTextCodec>
#include <QLineEdit>
#include <QKeyEvent>
#include <memory>

class TagInput : public QLineEdit {
	Q_OBJECT
public:
	explicit TagInput(QWidget* parent = 0);
	void fixTags(bool nosort = false);
	void loadTagFile(const QString& file);
	void reloadAdditionalTags();

protected:
	void keyPressEvent(QKeyEvent* event);

private:
	bool next_completer();
	QStringList parse_tags_file(QTextStream& input);

	void remove_if_short(QString &tag);
	void remove_if_unwanted(QString& tag);
	void replace_booru_and_id(QString& tag);

	QStringList related_tags(const QString& tag);
	QStringList replacement_tags(QString& tag);

	int m_index;
	std::unordered_multimap <QString,QString> m_related_tags;  // k: tag -- v: related tag
	std::unordered_multimap <QString,QString> m_replaced_tags; // k: replacement tag -- v: replaced tag
	std::unordered_map      <QString, bool>   m_removed_tags;  // k: removed tag -- v: been removed already?
	std::unique_ptr<MultiSelectCompleter>     m_completer;
};
#endif // TAGINPUT_H
