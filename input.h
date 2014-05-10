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
#include <QLineEdit>
#include <QKeyEvent>
#include <memory>

using QStringUnMap = std::unordered_multimap<QString,QString>;


class TagInput : public QLineEdit {
	Q_OBJECT
public:
	explicit TagInput(QWidget *parent = 0);
	void fixTags(bool nosort = false);
	void loadTagFile(const QString &file);
	void reloadMappedTags();

protected:
	void keyPressEvent(QKeyEvent *event);

private:
	bool nextCompleter();
	int index;

	QStringUnMap mapped_tags;
	std::unique_ptr<MultiSelectCompleter> completer;
};
#endif // TAGINPUT_H
