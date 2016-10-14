/* Copyright © 2011 Michał Męciński
 * Code taken from http://www.mimec.org/node/304
 */

#include "multicompleter.h"

#include <QLineEdit>

MultiSelectCompleter::MultiSelectCompleter(const QStringList& items, QObject* _parent)
	: QCompleter(items, _parent){}

MultiSelectCompleter::~MultiSelectCompleter(){}

QString MultiSelectCompleter::pathFromIndex(const QModelIndex& index) const
{
	auto path = QCompleter::pathFromIndex(index);
	auto text = qobject_cast<QLineEdit*>(widget())->text();
	int pos   = text.lastIndexOf(QChar(' '));

	if (pos >= 0) {
		path = text.left(pos) + QChar(' ') + path;
	}
	return path;
}

QStringList MultiSelectCompleter::splitPath(const QString& path) const
{
	int pos = path.lastIndexOf(QChar(' ')) + 1;
	const int path_length = path.length();

	while (pos < path_length && path[pos] == QChar(' ')) {
		++pos;
	}
	return QStringList(path.mid(pos));
}
