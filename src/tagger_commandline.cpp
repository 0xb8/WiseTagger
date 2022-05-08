/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLoggingCategory>
#include <QTextStream>
#include <QFileInfo>
#include <QFile>
#include "tagger_commandline.h"
#include "util/tag_file.h"
#include "util/strings.h"


namespace logging_category {
	Q_LOGGING_CATEGORY(cli, "TaggerCLI")
}
#define pdbg qCDebug(logging_category::cli)
#define pwarn qCWarning(logging_category::cli)

#ifdef Q_OS_WIN
	extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#else
	static int qt_ntfs_permission_lookup;
#endif


TaggerCommandLineInterface::TaggerCommandLineInterface()
{
	m_normal_tagfile_pattern = qEnvironmentVariable("WT_TAGFILE_NORMAL", QStringLiteral("*.tags.txt"));
	m_override_tagfile_pattern = qEnvironmentVariable("WT_TAGFILE_OVERRIDE", QStringLiteral("*.tags!.txt"));
}

int TaggerCommandLineInterface::parseArguments(QStringList args)
{
	QCommandLineParser parser;
	parser.setApplicationDescription(tr("WiseTagger: Simple picture tagging tool"));
	parser.addHelpOption();
	parser.addVersionOption();

	parser.addOptions({{"check", tr("Fix tags and print resulting file name.")},
	                   {"rename", tr("Fix tags, rename file and print resulting file name.")},
	                   {"remove-unknown-tags", tr("Remove unknown tags when fixing.")}});

	parser.addPositionalArgument("path", tr("Image file, directory or session file to open.\nUse \"-\" to read input file path from stdin."));

	parser.process(args);

	QTextStream out{stdout};
	QTextStream err{stderr};

	bool should_rename = parser.isSet(QStringLiteral("rename"));

	if (parser.isSet(QStringLiteral("check")) || should_rename) {

		auto filename = parser.positionalArguments();
		if (filename.size() != 1) {
			err << tr("Filename argument required.") << "\n\n";
			err.flush();
			parser.showHelp();
		}

		auto file = filename.first();
		if (file == "-") {
			QFile f;
			QTextStream stream;
			bool opened = f.open(stdin, QIODevice::ReadOnly|QIODevice::Text, QFileDevice::AutoCloseHandle);
			if(!opened) {
				pwarn << "parseArguments(): could not open stdin for reading";
				std::exit(1);
			}
			stream.setDevice(&f);
			stream.setCodec("UTF-8");
			file = stream.readLine();
		}

		QFileInfo fi{file};
		if (should_rename) {
			if (!fi.exists()) {
				err << tr("Input file [%1] does not exist.").arg(file) << '\n';
				err.flush();
				std::exit(1);
			}

			++qt_ntfs_permission_lookup;
			bool writable = QFileInfo{fi.path()}.isWritable();
			--qt_ntfs_permission_lookup;

			if (!writable) {
				err << tr("Input file [%1] is not renameable. Check directory permissions.").arg(file) << '\n';
				err.flush();
				std::exit(1);
			}
		}

		if (parser.isSet(QStringLiteral("remove-unknown-tags"))) {
			m_fix_opts.remove_unknown = true;
		}

		auto new_path = get_fixed_path(file);
		out << new_path << '\n';
		out.flush();

		if (parser.isSet(QStringLiteral("rename"))) {
			if (new_path != fi.filePath()) {
				Q_ASSERT(QFileInfo{new_path}.path() == fi.path());

				QFile f{file};
				bool res = f.rename(new_path);
				if (!res) {
					err << tr("Could not rename [%1] to [%2]").arg(file, new_path) << '\n';
					err.flush();
					std::exit(1);
				}
			}
		}
	} else {
		err << tr("No action specified.") << "\n\n";
		err.flush();

		parser.showHelp(1);
	}
	return 0;

}


QString TaggerCommandLineInterface::get_fixed_path(QString path)
{
	QFileInfo fi{path};
	QTextStream err{stderr};

	auto name = fi.completeBaseName();
	auto ext = fi.suffix();
	auto dir = fi.path();
	if (dir == QStringLiteral("."))
		dir.clear();

	std::vector<QDir> search_dir;
	QStringList tag_files, conflicting_files;
	util::find_tag_files_in_dir(QDir(dir), m_normal_tagfile_pattern, m_override_tagfile_pattern, search_dir, tag_files, conflicting_files);

	if (tag_files.isEmpty()) {
		err << tr("Could not find tag files.")    << "\n"
		    << tr("Looked in these directories:") << "\n";

		for (const auto &dir : search_dir) {
		       err << "\t" << dir.path() << "\n";
		}
		std::exit(1);
	}

    if (!conflicting_files.isEmpty()) {
        pwarn << "Conflicting tag files:" << conflicting_files;
    }

	QByteArray data;
	for(const auto& filename : qAsConst(tag_files)) {
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
			err << tr("Error opening tag file [%1].").arg(filename) << "\n";
			qApp->exit(1);
		}
		data.push_back(file.readAll());
		data.push_back('\n');
	}

	if (!m_parser.loadTagData(data)) {
		err << tr("Could not load tag data.") << '\n';
		std::exit(1);
	}

	TagEditState state;
	auto result = m_parser.fixTags(state, name, m_fix_opts);
	if (result.isEmpty()) {
		err << tr("Could not fix tags.") << '\n';
		std::exit(1);
	}

	auto new_name = util::join(result);
	if (!ext.isEmpty()) {
		new_name.append('.');
		new_name.append(ext);
	}

	if (!dir.isEmpty()) {
		new_name = QFileInfo{dir, new_name}.filePath();
	}

	return new_name;
}

