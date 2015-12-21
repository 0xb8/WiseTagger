/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGINPUT_H
#define TAGINPUT_H

#include <QStringList>
#include <QLineEdit>
#include <memory>

#include "multicompleter.h"
#include "util/unordered_map_qt.h"

class QKeyEvent;
class QTextStream;

class TagInput : public QLineEdit {
	Q_OBJECT
public:
	explicit TagInput(QWidget* _parent = nullptr);
	void fixTags(bool sort = true);
	void loadTagFiles(const QStringList& files);
	void reloadAdditionalTags();
	void setText(const QString &t);
	QString postURL() const;

signals:
	void postURLChanged(const QString&);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:

	bool        next_completer();
	QStringList parse_tags_file(QTextStream* input);
	QStringList related_tags(const QString& tag);
	QStringList replacement_tags(QString& tag);
	void        remove_if_unwanted(QString& tag);

	int         m_index;
	QStringList m_text_list;
	QString     m_post_url;
	std::unordered_multimap <QString,QString> m_related_tags;  // k: tag -- v: related tag
	std::unordered_multimap <QString,QString> m_replaced_tags; // k: replacement tag -- v: replaced tag
	std::unordered_map      <QString, bool>   m_removed_tags;  // k: removed tag -- v: been removed already?
	std::unique_ptr<MultiSelectCompleter>     m_completer;

	using tag_iterator = decltype(std::begin(m_text_list));
};
#endif // TAGINPUT_H
