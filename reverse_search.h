#ifndef REVERSESEARCH_H
#define REVERSESEARCH_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QProgressDialog>
#include <QFile>
#include <QString>
#include <QVector>
#include <QObject>

class ReverseSearch : public QObject
{

	Q_OBJECT
public:
	ReverseSearch();
	virtual ~ReverseSearch();

	void search(const QString &file);

private slots:
	void open_reply(QNetworkReply* reply);
	void showProgress(qint64 bytesSent, qint64 bytesTotal);

private:
	void upload_file();

	QString current_file;
	QVector<QString> response_files;
	QNetworkAccessManager nam;
	QProgressDialog upload_progress;
	QHttpMultiPart *multipart;

};

#endif // REVERSESEARCH_H
