/* Copyright © 2011 Michał Męciński
 * Code taken from http://www.mimec.org/node/304
 */

#include "multicompleter.h"
#include <QLineEdit>

MultiSelectCompleter::MultiSelectCompleter(const QStringList& items, QObject* _parent )
	: QCompleter( items, _parent )
{
}

MultiSelectCompleter::~MultiSelectCompleter()
{
}

QString MultiSelectCompleter::pathFromIndex( const QModelIndex& index ) const
{
	QString path = QCompleter::pathFromIndex( index );

	QString text = static_cast<QLineEdit*>( widget() )->text();

	int pos = text.lastIndexOf( ' ' );
	if ( pos >= 0 )
	path = text.left( pos ) + " " + path;

	return path;
}

QStringList MultiSelectCompleter::splitPath( const QString& path ) const
{
	int pos = path.lastIndexOf( ' ' ) + 1;

	while ( pos < path.length() && path.at( pos ) == QLatin1Char( ' ' ) )
	pos++;

	return QStringList( path.mid( pos ) );
}
