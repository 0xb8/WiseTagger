/* Copyright © 2019 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAG_FETCHER_H
#define TAG_FETCHER_H

#include <QString>
#include <QNetworkAccessManager>

class QNetworkReply;

class TagFetcher : public QObject {
	Q_OBJECT
public:
	TagFetcher(QObject* parent = nullptr);
	~TagFetcher();

	/*!
	 * \brief Fetches tags using imageboard JSON API
	 * \param url Imageboard API url for the post
	 */
	void fetch_tags(const QString& file, QString url);

	/*!
	 * \brief Aborts the tag fetching request
	 */
	void abort();

signals:

	/*!
	 * \brief Emitted when network error is detected while fetching tags
	 * \param error Human-readable error description
	 */
	void net_error(QUrl url, QString net_error);

	/*!
	 * \brief Emitted when tag fetching is started
	 */
	void started(QString url);

	/*!
	 * \brief Emitted when hashing input file.
	 */
	void hashing_progress(QString file, int percent);

	/*!
	 * \brief Emitted when \ref abort() is called.
	 */
	void aborted();

	/*!
	 * \brief Emitted when valid tags were found for
	 * \param tags Fetched tags
	 */
	void ready(QString file, QString tags);

	/*!
	 * \brief Emitted when non-network failure occured.
	 *
	 * For example, no such image exists on the imageboard, or MD5 mismatched.
	 */
	void failed(QString file, QString reason);


private:
	void open_reply(QNetworkReply* reply);

	/// file size threshold for displaying progress bar.
	static constexpr int64_t m_progress_threshold = 5 * 1024 * 1024;

	QNetworkAccessManager m_nam;
	QNetworkReply *m_reply = nullptr;
	bool m_hashing = false;
};


#endif // TAG_FETCHER_H
