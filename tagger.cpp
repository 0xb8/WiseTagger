/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "tagger.h"
#include <QMessageBox>
#include <QDirIterator>
#include <QApplication>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>


static const char* ConfigFilename = "config.json";

Tagger::Tagger(QWidget *parent) :
	QWidget(parent),
	pic(this),
	input(this)
{
	loadJsonConfig();
	mainlayout.setMargin(0);
	mainlayout.setSpacing(0);

	inputlayout.setMargin(10);
	inputlayout.addWidget(&input);

	hr_line.setFrameStyle(QFrame::HLine | QFrame::Sunken);
	mainlayout.addWidget(&pic);
	mainlayout.addWidget(&hr_line);
	mainlayout.addLayout(&inputlayout);
	setLayout(&mainlayout);
	setAcceptDrops(true);
}

Tagger::~Tagger() { }

void Tagger::installEventFilterForPicture(QObject * filter_object)
{
	pic.installEventFilter(filter_object);
}

void Tagger::reloadTags()
{
	input.loadTagFile(current_tags_file);
	input.reloadMappedTags();
}

QString Tagger::currentFile() const
{
	return current_file;
}

QString Tagger::currentFileName() const
{
	return QFileInfo(current_file).fileName();
}

bool Tagger::isModified() const
{
	return input.text() != QFileInfo(current_file).completeBaseName();
}

bool Tagger::loadFile(const QString &filename)
{
	QFileInfo f(filename);
	if(!f.exists()) {
		QMessageBox::critical(this, tr("Error opening image"), tr("File \"%1\" does not exist.").arg(f.fileName()));
		return false;
	}

	if(!pic.loadPicture(f.absoluteFilePath())) {
		QMessageBox::critical(this, tr("Error opening image"), tr("File format is not supported or file is corrupted."));
			return false;
	}

	input.setText(f.completeBaseName());
	current_file = f.absoluteFilePath();

	locateTagsFile(f);


	pic.setFocus();

	return true;


}

void Tagger::locateTagsFile(const QFileInfo& file)
{
	if(current_dir != file.absolutePath()) {
		current_dir = file.absolutePath();

		bool found = false;
		QFileInfo directory;

		for(auto&& entry : dir_tagfiles) {
			directory.setFile(entry.first);
			if(directory.exists() && current_dir.contains(directory.absoluteFilePath())) {
				current_tags_file = entry.second;
				found = true;
				break;
			}
		}
		if(!found) {
			auto it = dir_tagfiles.find("*");
			if(it != dir_tagfiles.end()) {
				current_tags_file = it->second;
			} else {
				current_tags_file = "tags.txt";
			}
		}
		input.loadTagFile(current_tags_file);
		return;
	}
	input.reloadMappedTags();
	return;
}



bool Tagger::loadJsonConfig()
{
	QString configfile = qApp->applicationDirPath() + '/' + ConfigFilename;

	QFile f(configfile);
	if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QMessageBox::warning(this, tr("Error opening config file"), tr("Could not open configuration file \"%1\"").arg(configfile));
		return false;
	}



	QJsonDocument config = QJsonDocument().fromJson(f.readAll());
	QJsonArray array = config.array();
	QJsonObject object;

	if(array.empty()) {
		QMessageBox::warning(this,
			tr("Error opening config file"),
			tr("\"%1\" is an empty or invalid JSON array\n\n%2")
			.arg(configfile)
			.arg(QString(QJsonDocument(array).toJson())));
		return false;
	}
	dir_tagfiles.clear();
	for(auto&& obj : array) {
		if(!obj.isObject()) {
			QMessageBox::warning(this, tr("Error opening config file"), tr("Array should contain valid JSON objects."));
			return false;
		}
		object = obj.toObject();
		if(!object.contains("directory") || !object.contains("tagfile")) {
			QMessageBox::warning(this,
				tr("Error opening config file"),
				tr("Object should have \"directory\" and \"tagfile\" keys\n\n%1")
				.arg(QString(QJsonDocument(object).toJson())));
			return false;
		}
		if(!object["directory"].isString() || !object["tagfile"].isString()) {
			QMessageBox::warning(this,
				tr("Error opening config file"),
				tr("<pre>directory</pre> and <pre>tagfile</pre> keys should be of type <pre>string</pre>."));
			return false;
		}

		dir_tagfiles.insert(std::pair<QString,QString>(object["directory"].toString(),object["tagfile"].toString()));
	} // for

	return true;
}



int Tagger::rename(bool forcesave)
{
	QFileInfo file(current_file);
	QString parent = file.canonicalPath(), ext = file.suffix();

	input.fixTags();
	QString newname = QFileInfo(parent + "/" + input.text() + "." + ext).filePath();
	if(newname == current_file) return 0;

	QFileInfo newfile(newname);
	if(newfile.exists()) {
		QMessageBox::critical(this, tr("Cannot rename file"),tr("File with this name already exists in \"<b>%1</b>\".\n\nPlease change some of your tags.").arg(QFileInfo(parent).completeBaseName()));
		return 0;
	}


	if(!forcesave) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Rename file?")
						, tr("Rename \"<b>%1</b>\"?").arg(file.completeBaseName())
						, QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
		if(reply == QMessageBox::Yes) {
			QFile(current_file).rename(newname);
			current_file = newname;
			return 1;
		}
		if(reply == QMessageBox::Cancel) {
			return -1;
		}

		return 0;

	} else {
		QFile(current_file).rename(newname);
		current_file = newname;
		return 1;
	}
}
