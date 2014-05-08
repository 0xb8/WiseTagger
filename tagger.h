#ifndef WISETAGGER_H
#define WISETAGGER_H

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QFrame>
#include <QWidget>
#include <QMap>

#include "picture.h"
#include "input.h"


class Tagger : public QWidget
{
	Q_OBJECT

public:
	explicit Tagger(QWidget *parent = nullptr);
	virtual ~Tagger();

	bool isModified() const;
	bool loadFile(const QString& filename);
	bool loadJsonConfig();
	bool createJsonConfig(const QString &directory);

	int rename(bool forcesave); // -1 when save is cancelled

	void installEventFilterForPicture(QObject *filter_object);
	void reloadTags();

	QString currentFile() const;
	QString currentFileName() const;

private:
	Picture pic;
	TagInput input;
	QFrame hr_line;
	QString current_file, current_dir, current_tags;
	QVBoxLayout mainlayout;
	QVBoxLayout inputlayout;
	QMap<QString,QString> directory_tagfiles;

};

#endif // WISETAGGER_H
