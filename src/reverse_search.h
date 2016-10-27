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
#include <QNetworkProxy>
#include <QHttpPart>
#include <QString>
#include <QVector>
#include <QFile>
#include <QUrl>

class QNetworkReply;
class QObject;

/*!
 * Class for reverse-searching image by uploading it to https://iqdb.org
 */
class ReverseSearch : public QObject
{
	Q_OBJECT
public:
	explicit ReverseSearch(QWidget *_parent = nullptr);
	~ReverseSearch() override;

public slots:
	/*!
	 * \brief Reverse search file.
	 * \param file Image to search for.
	 */
	void	search(const QString &file);


	/*!
	 * \brief Specify proxy URL to use.
	 *
	 * Proxy URL must start with valid scheme (http:// or socks://) and end
	 * with valid port number (0 < port <= 65535).
	 *
	 * \a Example: \c http://proxy.example.com:8080
	 */
	void	setProxy(const QUrl& proxy_url);


	/*!
	 * \brief Enables or disables proxy.
	 * \param enable Enable proxy.
	 */
	void	setProxyEnabled(bool enable);

	/*!
	 * \brief Updates proxy configuration from QSettings.
	 */
	void updateSettings();
public:

	/*!
	 * \retval true Proxy is enabled.
	 * \retval false Proxy is disabled.
	 */
	bool	proxyEnabled() const;

	/// Returns current proxy URL.
	QString	proxyURL() const;

	/// Returns current proxy.
	QNetworkProxy proxy() const;

signals:

	/// Upload finished.
	void finished();


	/*!
	 * \param[out] bytesSent Number of bytes sent.
	 * \param[out] bytesTotal Number of bytes total.
	 */
	void uploadProgress(qint64 bytesSent, qint64 bytesTotal);

private slots:
	void open_reply(QNetworkReply* reply);

private:
	struct IqdbHttpParts {
		IqdbHttpParts();
		QHttpPart maxsize;
		QHttpPart service;
		QHttpPart file;
		QHttpPart url;
	};

	static constexpr const char * const iqdb_url = "https://iqdb.org/";
	static constexpr const char * const iqdb_supported_formats = "JPEG, PNG, GIF";
	static constexpr size_t iqdb_max_file_size = 8 * 1024 * 1024;
	static constexpr int iqdb_max_image_width  = 7500;
	static constexpr int iqdb_max_image_height = 7500;

	void upload_file();

	QString m_current_file_name;
	QFile m_image_file;
	QVector<QString> m_response_files;
	QNetworkAccessManager m_nam;
	QNetworkProxy m_proxy;
	QUrl m_proxy_url;
	bool m_proxy_enabled;
};

#endif // REVERSESEARCH_H
