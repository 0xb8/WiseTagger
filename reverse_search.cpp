#include "reverse_search.h"

#include <QDesktopServices>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>

ReverseSearch::ReverseSearch() : nam(this)
{
	upload_progress.setLabelText(tr("Uploading file..."));
	upload_progress.setAutoClose(true);
	upload_progress.setCancelButton(nullptr);

}

ReverseSearch::~ReverseSearch()
{
	for(int i = 0; i < response_files.size(); ++i) {
		QFile(response_files[i]).remove();
	}
}

void ReverseSearch::search(const QString &file)
{
	current_file = file;
	upload_file();
}

void ReverseSearch::upload_file()
{
	QFileInfo fileinfo(current_file);
	static const char* iqdb_post_url = "http://iqdb.org/";
	static const char* iqdb_max_file_size = "8388608";

	if(fileinfo.size() > QString(iqdb_max_file_size).toInt()) {
		QMessageBox::warning(nullptr, tr("File is too large"), tr("File is too large. Maximum file size: 8Mb"));
		return;
	}

	QUrl post_url(iqdb_post_url);
	QNetworkRequest post_request(post_url);


	multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);
	QHttpPart maxsize;
	maxsize.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"MAX_FILE_SIZE\"");
	maxsize.setBody(iqdb_max_file_size);
	QVector<QHttpPart> service(15);

	for(int i = 0; i <= 12; ++i) {
		//service.push_back(QHttpPart());
		if(i > 6 && i < 10) continue;
		service[i].setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"service[]\"");
		service[i].setBody(QString::number(i+1).toLatin1());
	}

	QHttpPart filepart;
	filepart.setHeader(QNetworkRequest::ContentDispositionHeader,QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileinfo.completeBaseName()).toLatin1());
	filepart.setHeader(QNetworkRequest::ContentTypeHeader, QMimeDatabase().mimeTypeForFile(fileinfo).name().toLatin1());

	QFile *imagefile = new QFile(current_file, this);
	if (!imagefile->open(QIODevice::ReadOnly)){
		QMessageBox::critical(nullptr, tr("Cannot open file"), tr("Cannot open file %1 for uploading.").arg(current_file));
		return;
	}
	filepart.setBodyDevice(imagefile);
	imagefile->setParent(multipart);

	QHttpPart urlpart;
	urlpart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"url\"");
	urlpart.setBody("http://");

	multipart->append(maxsize);
	for(int i = 0; i< service.size(); ++i) {
		multipart->append(service[i]);
	}
	multipart->append(filepart);
	multipart->append(urlpart);

	connect(&nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(open_reply(QNetworkReply*)));
	QNetworkReply* r = nam.post(post_request, multipart);

	connect(r, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(showProgress(qint64,qint64)));
	upload_progress.show();

}


void ReverseSearch::open_reply(QNetworkReply* reply)
{
	if(reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) {
		QByteArray response = reply->readAll();
		QFileInfo curr_file(current_file);
		QString response_filename = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		response_filename.append("/iqdb_");
		response_filename.append(curr_file.completeBaseName());
		response_filename.append(".html");
		response_files.push_back(response_filename);

		QFile rf(response_filename);
		rf.open(QIODevice::WriteOnly);
		rf.write(response);
		QDesktopServices::openUrl(QUrl::fromLocalFile(response_filename));
	}
	reply->deleteLater();
	multipart->deleteLater();
}

void ReverseSearch::showProgress(qint64 bytesSent, qint64 bytesTotal)
{
	upload_progress.setMaximum(bytesTotal);
	upload_progress.setValue(bytesSent);
}
