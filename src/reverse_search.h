/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef REVERSESEARCH_H
#define REVERSESEARCH_H


/** @file reverse_search.h
 *  @brief Class @ref ReverseSearch
 */

#include <QNetworkAccessManager>
#include <QProgressDialog>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QString>
#include <QVector>
#include <QFile>
#include <QUrl>
#include <QByteArray>

class QNetworkReply;
class QObject;


/*!
 * \brief Implements image reverse-search on the web
 *
 * The \ref ReverseSearch class uploads specified image file by uploading
 * it to to https://iqdb.org and retrieving the results HTML document,
 * which can then be displayed in system-default browser.
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
	void search(const QString &file);

signals:

	/*!
	 * \brief Emitted when network error is detected while uploading image.
	 * \param url Request URL
	 * \param error Human-readable error description
	 */
	void error(QUrl url, QString error);

	/// Emitted when upload has finished.
	void finished();

	/// Emitted when reverse search has sucessfully finished.
	void reverseSearched();

	/*!
	 * \brief Emitted while uploading the file.
	 * \param bytesSent Number of bytes sent.
	 * \param bytesTotal Number of bytes total.
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
	static constexpr qint64 iqdb_max_file_size = 8 * 1024 * 1024;
	static constexpr int iqdb_max_image_width  = 7500;
	static constexpr int iqdb_max_image_height = 7500;

	void upload_file();

	QString m_current_file_name;
	QFile m_image_file;
	QByteArray m_image_scaled_data;
	QVector<QString> m_response_files;
	QNetworkAccessManager m_nam;
};

#endif // REVERSESEARCH_H
