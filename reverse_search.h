/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef REVERSESEARCH_H
#define REVERSESEARCH_H

#include <QNetworkAccessManager>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QHttpPart>
#include <QProgressDialog>
#include <QString>
#include <QVector>
#include <QObject>
#include <QFile>

struct IqdbHttpParts {
	IqdbHttpParts();
	QHttpPart maxsize;
	QHttpPart service;
	QHttpPart file;
	QHttpPart url;
};

class ReverseSearch : public QObject
{
	Q_OBJECT
public:
	explicit ReverseSearch(QObject* _parent = 0);
	~ReverseSearch();
	void search(const QString &file);

	const char* iqdb_post_url = "http://iqdb.org/";
	static const int iqdb_max_file_size = 8388608; // bytes

private slots:
	void open_reply(QNetworkReply* reply);
	void showProgress(qint64 bytesSent, qint64 bytesTotal);

private:
	void upload_file();

	QString current_file;
	QFile imagefile;
	QVector<QString> response_files;
	QNetworkAccessManager network_access_manager;
	QProgressDialog upload_progress;
};

#endif // REVERSESEARCH_H
