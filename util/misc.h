/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

/** @dir util
 * @brief Misc utilities
 */

/**
 * @file misc.h
 * @brief Miscellaneous utilities
 */

#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <QByteArray>
#include <QStringList>
#include <QIcon>
#include <QLocale>
#include <type_traits>
#include <stdint.h>

/**
 * @namespace util
 * @brief Miscellaneous utilities
 */
namespace util {

/// Read resource HTML file contents
QString                 read_resource_html(const char* filename);

/// language saved in settings
QLocale::Language       language_from_settings();

/// Language code corresponding to language \p name string
QLocale::Language       language_code(const QString& name);

/// Language name string from language \p code
QString                 language_name(QLocale::Language code);

/// Load icon from executable file (windows-only)
QIcon                   get_icon_from_executable(const QString& path);

/// All supported image formats filter list
QStringList             supported_image_formats_namefilter();

/// All supported video formats filter list
QStringList             supported_video_formats_namefilter();

/// Guessed image format for \p filename
QByteArray              guess_image_format(const QString& filename);

/// Save current settings into \p path file
bool                    backup_settings_to_file(const QString& path);

/// Load settings from \p path
bool                    restore_settings_from_file(const QString& path);

/// Returns (hopefully) unique file identifier based on device id, inode and mtime.
uint64_t                get_file_identifier(const QString& path);

/*!
 * \brief Returns whether \p path1 and \p path2 represent the same file.
 *
 * Files are compared by inode. In a case-insensitive filesystem, paths
 * differing only in case are necessarily representing the same file.
 *
 * \retval  0 - paths represent the same file
 * \retval  1 - paths represent different files
 * \retval -1 - error opening the file(s)
 */
int                    is_same_file(const QString& path1, const QString& path2);

/*!
 * \brief Attempt to resolve a symlink target file recursively.
 *
 * Since on live system any of intermediate links can change while resolving,
 * there is no 100% reliable way to resolve the target path.
 *
 * \param[in]  path        Symlink path.
 * \param[in]  max_depth   Maximum recursion depth for resolving.
 * \param[out] sequence    Intermediate symbolic links (optional).
 *
 * \return                 Resolved file path or empty string on error.
 */
QString                resolve_symlink_to_file(const QString& path, int max_depth=30, QStringList* sequence = nullptr);

} // namespace util

#endif // UTIL_MISC_H
