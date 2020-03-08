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
#include <QFile>
#include <QCryptographicHash>


TagFetcher::TagFetcher(QObject * parent) : QObject(parent) {
	connect(&m_nam, &QNetworkAccessManager::finished, this, &TagFetcher::open_reply);
}

TagFetcher::~TagFetcher() {}

void TagFetcher::fetch_tags(const QString & filename, QString url) {
	QCryptographicHash hash{QCryptographicHash::Md5};
	QFile file{filename};
	file.open(QIODevice::ReadOnly);
	hash.addData(&file);
	auto hash_hex = hash.result().toHex();

	if (url.isEmpty()) {
		url = QStringLiteral("https://capi-v2.sankakucomplex.com/posts?tags=md5:");
		// "https://danbooru.donmai.us/posts.json?tags=md5:"
		url.append(hash_hex);
	}

	QNetworkRequest req{url};
	req.setAttribute(QNetworkRequest::User, filename);
	req.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), hash_hex);
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
	QString res, res_md5;
	if (!doc.isNull()) {
		auto arr = doc.array();
		if (!arr.isEmpty()) {
			if (arr.first().isObject()) {
				auto post = arr.first().toObject();

				auto md5 = post.find("md5");
				if (md5 != post.end()) {
					res_md5 = md5->toString();
				}

				auto tags = post.find("tags");
				if (tags != post.end()) {

					if (tags->isString())
						res = tags->toString();
					else if (tags->isArray()) {
						// parse sankaku API
						auto arr = tags->toArray();
						for (auto val : arr) {
							if (val.isObject()) {
								auto obj = val.toObject();
								auto tag = obj.find("name_en");
								if (tag != obj.end() && tag->isString()) {
									if (!res.isEmpty()) {
										res.append(' ');
									}
									res.append(tag->toString());
								}
							}
						}

					}
				} else if (post.find("tag_string") != post.end()) {
					tags = post.find("tag_string");
					res = tags->toString();
				}
			}
		}
	}
	QUrl reply_url = reply->url();
	reply->close();
	reply->deleteLater();
	m_reply = nullptr;

	if (!res.isEmpty()) {
		auto file = reply->request().attribute(QNetworkRequest::User).toString();
		auto src_md5 = reply->request().attribute(QNetworkRequest::Attribute(QNetworkRequest::User+1)).toString();
		if (src_md5 != res_md5) {
			emit error(reply_url, tr("Image MD5 mismatch! Perhaps the file is corrupted?"));
		} else {
			emit ready(file, res);
		}
	}
}

