/* Copyright © 2019 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "util/tag_fetcher.h"
#include "util/network.h"
#include "util/strings.h"
#include <QSettings>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QCryptographicHash>


namespace logging_category {Q_LOGGING_CATEGORY(tag_fetcher, "TagFetcher")}
#define pdbg qCDebug(logging_category::tag_fetcher)
#define pwarn qCWarning(logging_category::tag_fetcher)


TagFetcher::TagFetcher(QObject * parent) : QObject(parent) {
	connect(&m_nam, &QNetworkAccessManager::finished, this, &TagFetcher::open_reply);
}



void TagFetcher::fetch_tags(const QString & filename, std::array<int, 5> imageboards) {
	QCryptographicHash hash{QCryptographicHash::Md5};
	QFile file{filename};
	file.open(QIODevice::ReadOnly);
	const auto file_size = file.size();
	if (file_size < m_progress_threshold) {
		hash.addData(&file);
	} else {
		auto data = reinterpret_cast<const char*>(file.map(0, file_size));
		if (!data) {
			emit failed(filename, tr("Could not map file."));
			return;
		}

		m_hashing = true;

		const auto chunk_size = file_size / 100;
		int64_t offset = 0;
		for (int i = 0; i < 100; ++i) {
			if (!m_hashing) return;
			hash.addData(data + offset, chunk_size);
			offset += chunk_size;
			emit hashing_progress(filename, i+1);
			qApp->processEvents();
		}
		auto remaining = file_size - offset;
		if (remaining > 0)
			hash.addData(data + offset, remaining);

	}
	const auto hash_hex = QString::fromLatin1(hash.result().toHex());

	// ---------------

	auto urls = QList<QUrl>();

	QSettings sett;
	sett.beginGroup("fetch_tags_from");
	if (imageboards[0] == 1 || (imageboards[0] == -1 && sett.value("yandere", true).toBool())) {
		urls.append(QStringLiteral("https://yande.re/post?tags=md5:%1").arg(hash_hex));
	}
	if (imageboards[1] == 1 || (imageboards[1] == -1 && sett.value("konachan", true).toBool())) {
		urls.append(QStringLiteral("https://konachan.com/post?tags=md5:%1").arg(hash_hex));
	}
	if (imageboards[2] == 1 || (imageboards[2] == -1 && sett.value("sankaku", true).toBool())) {
		urls.append(QStringLiteral("https://sankakuapi.com/posts?tags=md5:%1").arg(hash_hex));
	}
	if (imageboards[3] == 1 || (imageboards[3] == -1 && sett.value("danbooru", true).toBool())) {
		urls.append(QStringLiteral("https://danbooru.donmai.us/posts.json?md5=%1").arg(hash_hex));
	}
	if (imageboards[4] == 1 || (imageboards[4] == -1 && sett.value("gelbooru", true).toBool())) {
		urls.append(QStringLiteral("https://gelbooru.com/index.php?page=dapi&s=post&q=index&json=1&tags=md5:%1").arg(hash_hex));
	}
	sett.endGroup();

	// ---------------

	abort();

	if (urls.isEmpty()) {
		emit failed(filename, tr("All imageboards are disabled.\n"
		                         "You can enable them in the settings or in the tag file."));
		return;
	}

	for (const auto& url : qAsConst(urls)) {
		QNetworkRequest req{url};
		req.setAttribute(QNetworkRequest::User, filename);
		req.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), hash_hex);
		// NOTE: wtf? danbooru's cloudflare inserts captcha page into api requests for a browser UA
		req.setHeader(QNetworkRequest::UserAgentHeader, url.host().contains(QStringLiteral("danbooru")) ? "notabrowser" : WISETAGGER_USERAGENT);
		req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));

		auto reply = m_nam.get(req);
		m_replies.append(reply);
	}

	emit started(urls);
}

void TagFetcher::abort()
{
	m_hashing = false;
	if (!m_replies.isEmpty()) {
		for (auto& reply : m_replies) {
			reply->abort();
			reply->deleteLater();
		}
		m_replies.clear();
	}
	m_fetched_tags.clear();
	m_errors.clear();
	m_net_errors.clear();
	emit aborted();
}

void TagFetcher::open_reply(QNetworkReply * reply)
{
	if (reply->error() != QNetworkReply::NetworkError::NoError) {
		m_net_errors.append({reply->url(), reply->errorString()});
	}

	const auto data = reply->readAll();
	const auto doc = QJsonDocument::fromJson(data);
	QString res, res_md5;

	auto parse_post = [&res, &res_md5](const QJsonObject& post){
		const auto md5 = post.find("md5");
		if (md5 != post.end()) {
			res_md5 = md5->toString();
		}

		auto tags = post.find("tags");
		if (tags != post.end()) {
			if (tags->isString()) {
				res = tags->toString();
			} else if (tags->isArray()) {
				// parse sankaku API
				auto arr = tags->toArray();
				for (auto val : arr) {
					if (val.isObject()) {
						auto obj = val.toObject();
						auto tag = obj.find("tagName");
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
	};


	if (!doc.isNull()) {
		if (doc.isArray()) {
			const auto arr = doc.array();
			if (!arr.empty() && arr.first().isObject()) {
				const auto post = arr.first().toObject();
				parse_post(post);
			}

		} else if (doc.isObject()) {
			const auto docobj = doc.object();
			auto post = docobj.find("post");
			if (post != docobj.end()) {
				if (post->isObject()) {
					parse_post(post->toObject());
				} else if (post->isArray()) {
					parse_post(post->toArray().first().toObject());
				} else {
					pdbg << "post is neither object or array:" << post.value();
				}
			} else {
				parse_post(docobj);
			}
		}
	}
	reply->close();

	if (!res.isEmpty()) {
		auto src_md5 = reply->request().attribute(QNetworkRequest::Attribute(QNetworkRequest::User+1)).toString();
		if (src_md5 != res_md5) {
			m_errors.append({reply->url(), tr("Image MD5 mismatch! Perhaps the file is corrupted?")});
		} else {
			pdbg << "found tags in" << reply->url().host() << res;
			m_fetched_tags.append(res);
		}
	} else {
		pdbg << "empty result from" <<  reply->url().host();
		m_errors.append({reply->url(), tr("Image not found on the imageboard.")});
	}

	const auto file = reply->request().attribute(QNetworkRequest::User).toString();
	check_result(file);
}

void TagFetcher::check_result(const QString &file)
{
	if (std::all_of(m_replies.begin(), m_replies.end(), [](auto& reply) { return reply->isFinished(); })) {

		if (!m_fetched_tags.isEmpty()) {

			QString result;
			QSet<QString> found;

			for (const auto& tag_str : qAsConst(m_fetched_tags)) {
				auto tag_list = util::split(tag_str);

				for (const auto& tag : qAsConst(tag_list)) {
					if (!found.contains(tag)) {
						found.insert(tag);
						result.append(tag);
						result.append(QChar(' '));
					}
				}
			}
			emit ready(file, result.trimmed());

			for (auto e : m_errors) {
				pdbg << "Got API error:" << e;
			}

			for (auto& e : m_net_errors) {
				pdbg << "Got network errror:" << e.first << e.second;
			}

		} else {

			if (!m_errors.isEmpty()) {

				QString err_str;
				for (const auto& e : qAsConst(m_errors)) {
					err_str.append(QStringLiteral("%1: %2\n").arg(e.first.host(), e.second));
				}
				emit failed(file, err_str);
			}

			if (!m_net_errors.isEmpty()) {
				for (auto& e : m_net_errors) {
					emit net_error(e.first, e.second);
				}
			}
		}
	}
}

