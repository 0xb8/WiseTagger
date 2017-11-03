/* Copyright © 2011 Michał Męciński
 * Code taken from http://www.mimec.org/node/304
 */

#ifndef MULTICOMPLETER_H
#define MULTICOMPLETER_H

#include <QCompleter>
#include <QStringList>

class MultiSelectCompleter : public QCompleter
{
	Q_OBJECT
public:
	using QCompleter::QCompleter;
	~MultiSelectCompleter() override = default;

public:
	QString pathFromIndex(const QModelIndex& index) const override;
	QStringList splitPath(const QString& path) const override;
};

#endif // MULTICOMPLETER_H
