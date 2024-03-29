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
#include <QMimeDatabase>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QFileInfo>
#include <QBuffer>
#include <stdexcept>
#include <memory>
#include "util/size.h"
#include "util/network.h"

namespace logging_category {
	Q_LOGGING_CATEGORY(revsearch, "ReverseSearch")
}
#define pdbg qCDebug(logging_category::revsearch)
#define pwarn qCWarning(logging_category::revsearch)

ReverseSearch::ReverseSearch(QWidget *_parent) : QObject(_parent) { }

ReverseSearch::~ReverseSearch()
{
	for(auto f : qAsConst(m_response_files)) {
		QFile::remove(f);
	}
}

void ReverseSearch::search(const QString &file)
{
	m_current_file_name = file;
	upload_file();
}

void ReverseSearch::upload_file()
{
	QFileInfo file_info(m_current_file_name);

	m_image_file.setFileName(m_current_file_name);
	if (!m_image_file.open(QIODevice::ReadOnly)){
		QMessageBox::critical(
			nullptr,
			tr("Cannot open file"),
			tr("<p>Reverse search failed: Cannot open file <b>%1</b> for uploading.</p>")
				.arg(m_current_file_name));
		return;
	}

	{ // new scope since we don't need those afterwards
		QImageReader reader(&m_image_file);
		const auto format = reader.format();
		QByteArray formats{iqdb_supported_formats};
		if(!formats.contains(format.toUpper())) {
			QMessageBox::critical(nullptr,
				tr("Reverse search"),
				tr("<p>Reverse search of <b>%1</b> failed: Unsupported file format.</p><p>Supported formats are: <b>%2</b>.</p>")
					.arg(file_info.fileName())
					.arg(iqdb_supported_formats));
			return;
		}

		auto dimensions = reader.size();
		if(!dimensions.isValid()) {
			pwarn << "Could not determine image dimensions";
			return;
		}

		auto image_data_size = file_info.size();
		QImage image;
		while (dimensions.width() > iqdb_max_image_width
		       || dimensions.height() > iqdb_max_image_height
		       || image_data_size > iqdb_max_file_size)
		{
			int max_width = iqdb_max_image_width; // BUG: libstd++'s std::min() does not like this variable in debug mode
			auto half_width = std::min(dimensions.width(), max_width) / 2;
			if (image.isNull())
				image = reader.read();

			auto scaled = image.scaledToWidth(half_width, Qt::SmoothTransformation);
			dimensions = scaled.size();
			if (!dimensions.isValid()) {
				pwarn << "Could not determine scaled image dimensions";
				return;
			}

			m_image_scaled_data.clear();
			QBuffer buffer{&m_image_scaled_data};
			QImageWriter writer{&buffer, format};
			if (!writer.write(scaled)) {
				pwarn << "Could not write scaled image:" << writer.errorString();
				m_image_scaled_data.clear();
				return;
			}
			image_data_size = m_image_scaled_data.size();
			pdbg << "Scaled" << m_image_file.fileName() << "to" << dimensions << "/" << image_data_size << "bytes";
		}

	}
	m_image_file.reset(); // NOTE: avoid breaking file upload

	QNetworkRequest post_request(QUrl{iqdb_url});
	post_request.setHeader(QNetworkRequest::UserAgentHeader, WISETAGGER_USERAGENT);

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

	if (m_image_scaled_data.isEmpty()) {
		iqdb_parts.file.setBodyDevice(&m_image_file);
	} else {
		iqdb_parts.file.setBody(m_image_scaled_data);
	}

	/* Check IQDB search services
	 * NOTE: these ids correspond to iqdb.org service ids, perhaps we could preload
	 * them before searching and let user choose which ones to use.
	 */
	const auto ids = {1,2,3,4,5,6,10,11,13};
	for(auto id : ids) {
		iqdb_parts.service.setBody(QString::number(id).toLatin1());
		multipart->append(iqdb_parts.service);
	}
	multipart->append(iqdb_parts.maxsize);
	multipart->append(iqdb_parts.file);
	multipart->append(iqdb_parts.url);

	auto r = m_nam.post(post_request, multipart.get());
	multipart->setParent(r);
	multipart.release();
	connect(r, &QNetworkReply::uploadProgress, this, &ReverseSearch::uploadProgress);
	connect(r, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, [this](auto)
	{
		auto r = qobject_cast<QNetworkReply*>(this->sender());
		if (r) {
			emit this->error(r->url(), r->errorString());
		}
	});
	connect(r, &QNetworkReply::finished, this, [this, r]()
	{
		open_reply(r);
	});
}

void ReverseSearch::open_reply(QNetworkReply* reply)
{
	// use unique_ptr with custom deleter to clean up the request object
	auto guard = std::unique_ptr<QNetworkReply, std::function<void(QNetworkReply*)>>(reply, [](auto ptr)
	{
		pwarn << "delete later";
		ptr->deleteLater();
	});

	if (reply->operation() != QNetworkAccessManager::PostOperation)
		return;

	/* At this point request been sent, so we don't need this anymore */
	m_image_file.close();
	m_image_scaled_data.clear();

	if(reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) {
		QByteArray response = reply->readAll();
		QFileInfo curr_file(m_current_file_name);
		auto base_name = curr_file.completeBaseName();

		// FIXME: DIRTY HACK - thx iqdb...
		response.replace("<img src=\'/", "<img src=\'https://iqdb.org/");
		response.replace("<a href=\"//", "<a href=\"https://");
		response.replace("<title>Multi-service image search - Search results</title>",
				 "<title>WiseTagger Reverse Search Results</title>");

		// FIXME: another dirty hack: inserting this js function to one-click-copy tags
		auto onclick = R"~(<th>Best match</br>(<a href="#" onclick="
									javascript:
										var tagsString = document.querySelectorAll('td.image a img')[0].getAttribute('title').split('Tags: ')[1];
										if (tagsString.length > -1) {
											const el = document.createElement('textarea');
											el.value = tagsString;
											document.getElementById('pages').appendChild(el);
											el.select();
											document.execCommand('copy');
											document.getElementById('pages').removeChild(el);
										} else {
											console.log('No tags to copy.');
										};
									">copy tags</a>)</th>)~";

		response.replace("<th>Best match</th>", onclick);


		QString response_filename = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		response_filename.append(QStringLiteral("/WT_Search_Results_For_"));
		response_filename.append(base_name.replace(' ', '_'));
		response_filename.truncate(240);
		response_filename.append(QStringLiteral(".html"));
		m_response_files.push_back(response_filename);

		QSaveFile rf(response_filename);
		rf.setDirectWriteFallback(true);
		rf.open(QIODevice::WriteOnly);
		rf.write(response);
		if(rf.commit()) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(response_filename));
			emit reverseSearched();
		}
	} else {
		pwarn << "open_reply(): incorrect reply:" << reply->errorString();
	}
	emit finished();
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
