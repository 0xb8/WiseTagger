#include "tagger.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDirIterator>
#include <QtDebug>

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
	mainlayout.addWidget(&pic);

	inputlayout.setMargin(10);
	inputlayout.addWidget(&input);
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
	input.loadTagFile(current_tags);
	input.reloadMappedTags();
}

QString Tagger::currentFile() const
{
	return current_file;
}

QString Tagger::currentFileName() const
{
	return QFileInfo(current_file).completeBaseName();
}

bool Tagger::isModified() const
{
	return input.text() != currentFileName();
}

bool Tagger::loadFile(const QString &filename)
{
	if(!pic.loadPicture(filename)) {
		QMessageBox::critical(this, tr("Error opening image"), tr("File is corrupted or file format is unrecognized."));
			return false;
	}
	QFileInfo f(filename);
	input.setText(f.completeBaseName());
	pic.setFocus();
	current_file = filename;

	if(current_dir == f.absolutePath()) {
		input.reloadMappedTags();
		return true;
	} else {
		current_dir = f.absolutePath();

		bool found = false;
		for(QString &k : directory_tagfiles.keys()) {
			QFileInfo kinfo(k);
			if(kinfo.exists() && current_dir.contains(kinfo.absoluteFilePath())) {
				current_tags = directory_tagfiles[k];
				found = true;
				break;
			}
		}
		if(!found) {
			if(directory_tagfiles.find("*") != directory_tagfiles.end())
				current_tags = directory_tagfiles["*"];
			else current_tags = "tags.txt";
		}
		input.loadTagFile(current_tags);
	}
	input.reloadMappedTags();
	return true;
}

bool Tagger::loadJsonConfig()
{
	QString configfile;
	if((configfile = QStandardPaths::locate(QStandardPaths::QStandardPaths::ConfigLocation, ConfigFilename)).isEmpty()) {
		if(!QFileInfo(ConfigFilename).exists()) {
			createJsonConfig(QStandardPaths::writableLocation(QStandardPaths::QStandardPaths::ConfigLocation));
			configfile = QStandardPaths::locate(QStandardPaths::QStandardPaths::ConfigLocation, ConfigFilename);
		} else {
			configfile = ConfigFilename;
		}
	}

	QFile f(configfile);
	if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QMessageBox::warning(this, tr("Error opening config file"), tr("Could not open configuration file \"%1\"").arg(configfile));
		return false;
	}

	QJsonDocument config = QJsonDocument().fromJson(f.readAll());

	QJsonArray tagdirs = config.array();
	if(tagdirs.empty()) {
		QMessageBox::warning(this, tr("Error opening config file"), tr("\"%1\" is an empty or invalid JSON array\n\n%2").arg(configfile).arg(QString(QJsonDocument(tagdirs).toJson())));
		return false;
	}
	directory_tagfiles.clear();
	for(auto&& obj : tagdirs) {
		if(!obj.isObject()) {
			QMessageBox::warning(this, tr("Error opening config file"), tr("Invalid object in config file."));
			return false;
		}
		QJsonObject tagdir = obj.toObject();
		if(!tagdir.contains("directory") || !tagdir.contains("tagfile")) {
			QMessageBox::warning(this, tr("Error opening config file"), tr("Object should have \"directory\" and \"tagfile\" keys\n\n%1").arg(QString(QJsonDocument(tagdir).toJson())));
			return false;
		}
		directory_tagfiles.insert(tagdir["directory"].toString(), tagdir["tagfile"].toString());
	}

	return true;
}

bool Tagger::createJsonConfig(const QString &directory) {
	QDir dir(directory);
	if(!dir.mkpath(".")) {
		QMessageBox::warning(this, tr("Error creating configuration directory"), tr("Could not create configuration directory %1.").arg(directory));
		return false;
	}

	QFileInfo fi(directory, ConfigFilename);
	QFile f(fi.absoluteFilePath());
	if(!f.open(QIODevice::ReadWrite|QIODevice::Text)) {
		QMessageBox::warning(this, tr("Error opening config file"), tr("Could not open configuration file \"%1\" for writing.").arg(f.fileName()));
		return false;
	}
	QJsonDocument config;
	QJsonArray tagdirs;
	QJsonObject tagdir;
	tagdir.insert(QString("directory"), QString("*"));
	tagdir.insert(QString("tagfile"), QString("tags.txt"));
	tagdirs.push_back(tagdir);
	config.setArray(tagdirs);
	f.write(config.toJson());
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
