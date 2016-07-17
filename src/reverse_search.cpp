/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "reverse_search.h"
#include <QLoggingCategory>
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QMimeDatabase>
#include <QMessageBox>
#include <QSettings>
#include <stdexcept>
#include <QFileInfo>
#include <memory>

namespace logging_category {
	Q_LOGGING_CATEGORY(revsearch, "ReverseSearch")
}
#define pdbg qCDebug(logging_category::revsearch)
#define pwarn qCWarning(logging_category::revsearch)

ReverseSearch::ReverseSearch(QWidget *_parent) : QObject(_parent), m_proxy(QNetworkProxy::NoProxy)
{
	updateSettings();

	connect(&m_nam, &QNetworkAccessManager::finished, this, &ReverseSearch::open_reply);
}

ReverseSearch::~ReverseSearch()
{
	for(const auto& f : m_response_files) {
		QFile::remove(f);
	}
}

void ReverseSearch::search(const QString &file)
{
	m_current_file_name = file;
	upload_file();
}

void ReverseSearch::setProxy(const QUrl &proxy_url)
{
	bool scheme_valid = (proxy_url.scheme() == QStringLiteral("http")
			  || proxy_url.scheme() == QStringLiteral("socks5"));

	if(!proxy_url.isValid() || proxy_url.port() == -1 || !scheme_valid)
	{
		QMessageBox::warning(nullptr,
			tr("Invalid proxy"),
			tr("<p>Proxy URL <em><code>%1</code></em> is invalid. Proxy <b>will not</b> be used!</p>").arg(proxy_url.toString()));
		return;
	}

	QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;
	if(proxy_url.scheme() == QStringLiteral("socks5"))
		type = QNetworkProxy::Socks5Proxy;
	else if(proxy_url.scheme() == QStringLiteral("http"))
		type = QNetworkProxy::HttpProxy;

	m_proxy = QNetworkProxy(type, proxy_url.host(), proxy_url.port());
	m_proxy_url = proxy_url;
	setProxyEnabled(true);
}

void ReverseSearch::setProxyEnabled(bool value)
{
	m_proxy_enabled = value;
	if(!m_proxy_enabled) {
		m_nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
	} else {
		m_nam.setProxy(m_proxy);
	}
}

bool ReverseSearch::proxyEnabled() const
{
	return m_proxy_enabled;
}

QString ReverseSearch::proxyURL() const
{
	return m_proxy_url.toString();
}

QNetworkProxy ReverseSearch::proxy() const
{
	return m_nam.proxy();
}

void ReverseSearch::updateSettings()
{
	QSettings settings;
	settings.beginGroup(QStringLiteral("proxy"));
	setProxyEnabled(settings.value(QStringLiteral("enabled"), false).toBool());

	if(m_proxy_enabled) {
		auto protocol = settings.value(QStringLiteral("protocol")).toString();
		auto host = settings.value(QStringLiteral("host")).toString();
		auto port = settings.value(QStringLiteral("port")).toInt();
		QUrl proxy_url;
		proxy_url.setScheme(protocol);
		proxy_url.setHost(host);
		proxy_url.setPort(port);
		setProxy(proxy_url);
	}
	settings.endGroup();
}

void ReverseSearch::upload_file()
{
	QFileInfo file_info(m_current_file_name);

	if(static_cast<size_t>(file_info.size()) > iqdb_max_file_size) {
		QMessageBox::warning(
			nullptr,
			tr("File is too large"),
			tr("<p>File is too large.</p><p>Maximum file size is <b>8Mb</b></p>"));
		return;
	}

	m_image_file.setFileName(m_current_file_name);
	if (!m_image_file.open(QIODevice::ReadOnly)){
		QMessageBox::critical(
			nullptr,
			tr("Cannot open file"),
			tr("<p>Cannot open file <b>%1</b> for uploading.</p>")
				.arg(m_current_file_name));
		return;
	}

	QNetworkRequest post_request(QUrl{iqdb_url});

	auto multipart = std::make_unique<QHttpMultiPart>(QHttpMultiPart::FormDataType);
	IqdbHttpParts iqdb_parts;

	/* Add filename, mime type and file data to file part */
	iqdb_parts.file.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
			.arg(file_info.completeBaseName()).toLatin1());

	iqdb_parts.file.setHeader(
		QNetworkRequest::ContentTypeHeader,
		QMimeDatabase().mimeTypeForFile(file_info).name().toLatin1());

	iqdb_parts.file.setBodyDevice(&m_image_file);

	/* Check IQDB search services */
	for(int i = 0; i < 12; ++i) {
		/* these id's are skipped on iqdb form */
		if(i > 6 && i < 10) continue;
		/* id = 0 is unchecked by default */
		iqdb_parts.service.setBody(QString::number(i+1).toLatin1());
		multipart->append(iqdb_parts.service);
	}
	multipart->append(iqdb_parts.maxsize);
	multipart->append(iqdb_parts.file);
	multipart->append(iqdb_parts.url);

	auto r = m_nam.post(post_request, multipart.get());
	multipart->setParent(r);
	multipart.release();
	connect(r, &QNetworkReply::uploadProgress, this, &ReverseSearch::uploadProgress);
}

void ReverseSearch::open_reply(QNetworkReply* reply)
{
	/* At this point request been sent, so we don't need this anymore */
	m_image_file.close();

	if(reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) {
		QByteArray response = reply->readAll();
		QFileInfo curr_file(m_current_file_name);
		auto base_name = curr_file.completeBaseName();

		// FIXME: DIRTY HACK - thx iqdb...
		response.replace("<img src=\'/", "<img src=\'https://iqdb.org/");
		response.replace("<a href=\"//", "<a href=\"https://");
		response.replace("<title>Multi-service image search - Search results</title>",
				 "<title>WiseTagger Reverse Search Results</title>");

		QString response_filename = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		response_filename.append(QStringLiteral("/WT_Search_Results_For_"));
		response_filename.append(base_name.replace(' ', '_'));
		response_filename.truncate(240);
		response_filename.append(QStringLiteral(".html"));
		m_response_files.push_back(response_filename);

		QFile rf(response_filename);
		rf.open(QIODevice::WriteOnly);
		rf.write(response);
		QDesktopServices::openUrl(QUrl::fromLocalFile(response_filename));
		emit finished();
	} else {
		pwarn << "open_reply(): incorrect reply:" << reply;
	}
	reply->deleteLater();
}

ReverseSearch::IqdbHttpParts::IqdbHttpParts()
{
	maxsize.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QStringLiteral("form-data; name=\"MAX_FILE_SIZE\""));

	service.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QStringLiteral("form-data; name=\"service[]\""));

	url.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QStringLiteral("form-data; name=\"url\""));

	maxsize.setBody(QString::number(ReverseSearch::iqdb_max_file_size).toLatin1());
	url.setBody("http://");
}
