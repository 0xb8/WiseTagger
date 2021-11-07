/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAG_FILE_H
#define TAG_FILE_H

#include <QDir>
#include <QString>
#include <vector>
#include <QStringList>

namespace util {

/*!
 * \brief Finds tag file in the \p current_dir and all parent directories.
 * \param current_dir Directory in which to start search
 * \param tagsfile Pattern describing normal tags file, e.g. "*.tags.txt"
 * \param override Pattern describing override tags file, e.g. "*.tags!.txt"
 * \param[out] search_dirs Directories where we looked for tag files.
 * \param[out] tags_files Resulting list of tags files, in root to leaf directory order
 */
void find_tag_files_in_dir(QDir current_dir,
                           const QString& tagsfile,
                           const QString& override,
                           std::vector<QDir>& search_dirs,
                           QStringList& tags_files);

}

#endif // TAG_FILE_H
