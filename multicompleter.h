#ifndef MULTICOMPLETER_H
#define MULTICOMPLETER_H

#include <QCompleter>
#include <QStringList>

class MultiSelectCompleter : public QCompleter
{
    Q_OBJECT
public:
    MultiSelectCompleter( const QStringList& items, QObject* parent );
    ~MultiSelectCompleter();

public:
    QString pathFromIndex( const QModelIndex& index ) const;
    QStringList splitPath( const QString& path ) const;
};

#endif // MULTICOMPLETER_H
