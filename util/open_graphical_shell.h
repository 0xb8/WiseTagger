/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef OPEN_GRAPHICAL_SHELL_H
#define OPEN_GRAPHICAL_SHELL_H
#include <QString>

/*!
 * \file open_graphical_shell.h
 * \brief Open file in GUI file manager.
 */

namespace util {

/// Open \p files in GUI file manager.
void open_files_in_gui_shell(const QStringList& files);

}

#endif // OPEN_GRAPHICAL_SHELL_H
