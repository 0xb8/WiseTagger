/* Copyright © 2019 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "util/tag_fetcher.h"
#include "util/network.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>


TagFetcher::TagFetcher(QObject * parent) : QObject(parent) {
	connect(&m_nam, &QNetworkAccessManager::finished, this, &TagFetcher::open_reply);
}

TagFetcher::~TagFetcher() {}

void TagFetcher::fetch_tags(const QString& url) {
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::UserAgentHeader, WISETAGGER_USERAGENT);

	if (m_reply) m_reply->deleteLater();
	abort();
	m_reply = m_nam.get(req);
	connect(m_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, [this](auto)
	{
		if (!m_reply) return;
		emit this->error(m_reply->url(), m_reply->errorString());
		m_reply->deleteLater();
		m_reply = nullptr;
	});
	emit started(url);
}

void TagFetcher::abort()
{
	if (m_reply) {
		m_reply->abort();
		m_reply->deleteLater();
		m_reply = nullptr;
	}
}

void TagFetcher::open_reply(QNetworkReply * reply)
{
	auto doc = QJsonDocument::fromJson(reply->readAll());
	QString res;
	if (!doc.isNull()) {
		auto arr = doc.array();
		if (!arr.isEmpty()) {
			auto post = arr.first().toObject();

			auto tags = post.find("tags");
			if (tags != post.end()) {
				res = tags->toString();
			}
		}
	}
	reply->close();
	reply->deleteLater();
	m_reply = nullptr;

	if (!res.isEmpty())
		emit ready(reply->url().toString(), res);
}

