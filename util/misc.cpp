/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QApplication>
#include <QDir>
#include <QDate>
#include <QFile>
#include <QImageReader>
#include <QSettings>
#include <QMetaEnum>
#include <stdexcept>
#include "util/misc.h"
#include "util/size.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <QtWin>


uint64_t util::get_file_identifier(const QString& path)
{
	static_assert (sizeof(wchar_t) == 2, "wchar_t is not UTF-16");
	auto native_path = QDir::toNativeSeparators(path);
	uint64_t ret = 0;

	HANDLE h = CreateFileW((wchar_t*)native_path.utf16(),
	                       0,
	                       FILE_SHARE_DELETE,
	                       nullptr,
	                       OPEN_EXISTING,
	                       FILE_ATTRIBUTE_NORMAL,
	                       nullptr);

	if(h == INVALID_HANDLE_VALUE)
		return 0u;

	BY_HANDLE_FILE_INFORMATION info;
	if(GetFileInformationByHandle(h, &info)) {
		uint64_t inode = ((uint64_t)info.nFileIndexHigh << 32) | (uint64_t)info.nFileIndexLow;
		uint64_t device = (uint64_t)info.dwVolumeSerialNumber << 32;
		uint64_t mtime = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) | (uint64_t)info.ftLastWriteTime.dwLowDateTime;
		uint64_t mtime_posix = mtime / 10000 - 11644473600ll; // close enough POSIX conversion
		ret = inode ^ device ^ (mtime_posix << 16);
	}
	CloseHandle(h);
	return ret;
}
#elif defined(Q_OS_UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t get_file_identifier(const QString &path)
{
	auto native_path = path.toLocal8Bit();
	uint64_t ret = 0u;

	struct stat sb;
	if(0 == stat(native_path.constData(), &sb)) {
		uint64_t inode = sb.st_ino;
		uint64_t device = sb.st_dev;
		uint64_t mtime = ((uint64_t)sb.st_mtim.tv_sec); // nanosec timestamps not supported by ntfs-3g
		ret = inode ^ (device << 32) ^ (mtime << 16);
	}
	return ret;
}
#else
#error "util::get_file_identifier() not implemented for this OS"
#endif


#define SETT_LOCALE     QStringLiteral("window/language")
#define SETT_LOCALE_OLD QStringLiteral("window/locale")
#define HTML_LOCATION   QStringLiteral(":/html/%1/%2")

QString util::read_resource_html(const char *filename)
{
	auto language = QLocale::languageToString(language_from_settings());

	QFile file(HTML_LOCATION.arg(language, filename));
	bool open = file.open(QIODevice::ReadOnly);
	if(!open) {
		file.setFileName(HTML_LOCATION.arg(QStringLiteral("English"), filename));
		open = file.open(QIODevice::ReadOnly);
	}
	if(!open)
		throw std::invalid_argument(std::string("read_resource_html():  ") + file.errorString().toStdString());

	QString ret(file.readAll());
	auto date = QDate::currentDate();
	if(strcmp(filename, "welcome.html") == 0 && date.month() == 10 && date.day() == 31) {
		ret.append(QStringLiteral("\n<p>In loving memory of my cat <b>Семён</b> (April 2000 - 31 October 2016).<br/><em>Requiescat In Pace</em>.</p>"));
	}
	return ret;
}

QIcon util::get_icon_from_executable(const QString &path)
{
#ifdef Q_OS_WIN
	#ifdef _MSC_VER
		#pragma comment( lib, "User32" )
	#endif
	auto widepath = QDir::toNativeSeparators(path).toStdWString();
	auto hinstance = ::GetModuleHandle(NULL);
	auto hicon = ::ExtractIcon(hinstance, widepath.c_str(), 0);
	auto ricon = QIcon();
	if(reinterpret_cast<uintptr_t>(hicon) > 1) { // NOTE: winapi derp
		ricon.addPixmap(QtWin::fromHICON(hicon));
	}
	::DestroyIcon(hicon);
	return ricon;
#else
	Q_UNUSED(path)
	return QIcon(); // TODO: load icon from .desktop file on linux
#endif
}

QString util::language_name(QLocale::Language lang)
{
	return QLocale::languageToString(lang);
}

QLocale::Language util::language_code(const QString& name)
{
	auto lang_meta = QMetaEnum::fromType<QLocale::Language>();
	bool ok = false;
	auto ret = lang_meta.keyToValue(qPrintable(name), &ok);
	if(ok) {
		return static_cast<QLocale::Language>(ret);
	}
	return QLocale::English;
}

QLocale::Language util::language_from_settings()
{
	QSettings settings;
	auto code = settings.value(SETT_LOCALE);
	if(!code.isValid()) {
		code = QLocale::English;
		settings.remove(SETT_LOCALE_OLD);
		settings.setValue(SETT_LOCALE, code);
	}
	return static_cast<QLocale::Language>(code.toInt());
}

bool util::backup_settings_to_file(const QString& path)
{
	QSettings new_settings(path, QSettings::IniFormat);
	new_settings.setIniCodec("UTF-8");
	QSettings old_settings;

	for(const auto& key : old_settings.allKeys()) {
		new_settings.setValue(key, old_settings.value(key));
	}
	new_settings.sync();
	return new_settings.status() == QSettings::NoError;
}

bool util::restore_settings_from_file(const QString & path)
{
	QSettings settings;
	QSettings file_settings(path, QSettings::IniFormat);
	file_settings.setIniCodec("UTF-8");
	file_settings.sync();
	if(file_settings.status() != QSettings::NoError)
		return false;

	settings.clear();
	for(const auto& key : file_settings.allKeys()) {
		settings.setValue(key, file_settings.value(key));
	}
	return true;
}

QStringList util::supported_image_formats_namefilter()
{
	static QStringList ret;
	if(ret.isEmpty()) {
		QString tmp;
		for(auto& ext : QImageReader::supportedImageFormats()) {
			if(ext == "pbm" // NOTE: who even uses these formats and why are they builtin
			|| ext == "pgm"
			|| ext == "ppm"
			|| ext == "xbm"
			|| ext == "xpm") continue;

			tmp = QStringLiteral("*.");
			tmp.append(ext);
			ret.push_back(tmp);
		}
	}
	return ret;
}

QStringList util::supported_video_formats_namefilter()
{
	static QStringList ret;
	if(ret.isEmpty()) {
		ret.push_back(QStringLiteral("*.avi"));
		ret.push_back(QStringLiteral("*.mp4"));
		ret.push_back(QStringLiteral("*.m4v"));
		ret.push_back(QStringLiteral("*.mkv"));
		ret.push_back(QStringLiteral("*.webm"));
		ret.push_back(QStringLiteral("*.wmv"));
		ret.push_back(QStringLiteral("*.3gp"));
		ret.push_back(QStringLiteral("*.mov"));
		ret.push_back(QStringLiteral("*.mpg"));
		ret.push_back(QStringLiteral("*.mpeg"));
		ret.push_back(QStringLiteral("*.mpe"));
		ret.push_back(QStringLiteral("*.ts"));
		ret.push_back(QStringLiteral("*.flv"));
		ret.push_back(QStringLiteral("*.f4v"));
	}
	return ret;
}

QByteArray util::guess_image_format(const QString& filename)
{
	const char * const exts[] = {
		"jpg", "jpeg",
		"png",
		"gif",
		"bmp",
		"svg",
		"tga",
		"tiff",
		"webp"
	};
	const char * const formats[] = {
		"jpg",
		"png",
		"gif",
		"bmp",
		"svg",
		"tga",
		"tiff",
		"webp"
	};
	const unsigned exts_to_formats[] = {
		0, 0, // jpeg
	        1,    // png
	        2,    // gif
	        3,    // bmp
	        4,    // svg
	        5,    // tga
	        6,    // tiff
	        7     // webp
	};

	QByteArray res;
	const auto ext = filename.mid(filename.lastIndexOf('.')+1).toLower();

	for(size_t i = 0; i < util::size::array_size(formats); ++i) {
		if(ext == exts[i]) {
			auto fmt_id = exts_to_formats[i];
			res.append(formats[fmt_id]);
			break;
		}
	}

	return res;
}


int util::is_same_file(const QString& path1, const QString& path2)
{
	// check if inode is the same
	auto id1 = get_file_identifier(path1);
	auto id2 = get_file_identifier(path2);

	if (id1 == 0 || id2 == 0)
		// could not open file(s)
		return -1;

	if (id1 == id2)
		return 0;

	return 1;
}

