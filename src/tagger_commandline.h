/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGGER_CLI_H
#define TAGGER_CLI_H

#include <QObject>
#include <QString>
#include <QDir>
#include "tag_parser.h"

class TaggerCommandLineInterface : public QObject
{
	Q_OBJECT
public:
	TaggerCommandLineInterface();
	~TaggerCommandLineInterface() = default;

	/*!
	 * \brief Parse command line arguments and check/fix tags if necessary.
	 * \param args Command line arguments.
	 * \retval 0 Successfully parsed arguments.
	 * \retval 1 Error.
	 */
	int parseArguments(QStringList args);

private:
	QString get_fixed_path(QString path);
	void init_parser(QDir dir);

	TagParser m_parser;
	TagParser::FixOptions m_fix_opts;

	QString m_normal_tagfile_pattern;
	QString m_override_tagfile_pattern;
};

#endif // TAGGER_CLI_H
