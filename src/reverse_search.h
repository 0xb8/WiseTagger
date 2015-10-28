/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef REVERSESEARCH_H
#define REVERSESEARCH_H

#include <QNetworkAccessManager>
#include <QProgressDialog>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QString>
#include <QVector>
#include <QFile>
#include <QUrl>

class QNetworkReply;
class QObject;

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
	explicit ReverseSearch(QWidget *_parent = nullptr);
	~ReverseSearch() override;

	void	search(const QString &file);
	void	setProxy(const QUrl& proxy_url);
	void	setProxyEnabled(bool);
	bool	proxyEnabled() const;
	QString	proxyURL() const;

	static constexpr const char * const iqdb_url = "http://iqdb.org/";
	static constexpr size_t iqdb_max_file_size = 8 * 1024 * 1024;

signals:
	void finished();
	void uploadProgress(qint64 bytesSent, qint64 bytesTotal);

private slots:
	void open_reply(QNetworkReply* reply);

private:
	void upload_file();
	void load_proxy_settings();

	QString m_current_file_name;
	QFile m_image_file;
	QVector<QString> m_response_files;
	QNetworkAccessManager m_nam;
	QNetworkProxy m_proxy;
	QUrl m_proxy_url;
	bool m_proxy_enabled;
};

#endif // REVERSESEARCH_H
