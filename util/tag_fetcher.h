/* Copyright © 2019 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAG_FETCHER_H
#define TAG_FETCHER_H

/**
 * \file tag_fetcher.h
 * \brief Class \ref TagFetcher
 */

#include <QString>
#include <QNetworkAccessManager>

class QNetworkReply;

/// Fetches tags from imageboard API
class TagFetcher : public QObject {
	Q_OBJECT
public:
	/*!
	 * \brief Constructs the Tag Fetcher
	 * \param parent Parent QObject.
	 */
	TagFetcher(QObject* parent = nullptr);
	~TagFetcher() override = default;

	/*!
	 * \brief Fetches tags using imageboard JSON API.
	 * \param file File for which to fetch tags.
	 * \param url Imageboard API url for the post.
	 */
	void fetch_tags(const QString& file, QString url);

	/*!
	 * \brief Aborts the tag fetching request.
	 */
	void abort();

signals:

	/*!
	 * \brief Emitted when network error is detected while fetching tags.
	 * \param url Imageboard API URL for this file.
	 * \param net_error Human-readable error description.
	 */
	void net_error(QUrl url, QString net_error);

	/*!
	 * \brief Emitted when tag fetching is started.
	 * \param url Imageboard API url for this file.
	 */
	void started(QString url);

	/*!
	 * \brief Emitted when hashing input file.
	 * \param file File for which tags are fetched.
	 * \param percent Percentage completed.
	 */
	void hashing_progress(QString file, int percent);

	/*!
	 * \brief Emitted when \ref abort() is called.
	 */
	void aborted();

	/*!
	 * \brief Emitted when valid tags were found.
	 * \param file File for which tags are fetched.
	 * \param tags Fetched tags
	 */
	void ready(QString file, QString tags);

	/*!
	 * \brief Emitted when non-network failure occured.
	 *
	 * \param file File for which tags are fetched.
	 * \param reason Failure reason.
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
