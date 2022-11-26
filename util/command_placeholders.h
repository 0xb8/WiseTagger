/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef COMMAND_PLACEHOLDERS_H
#define COMMAND_PLACEHOLDERS_H

#define SETT_COMMANDS_KEY       QStringLiteral("window/commands")
#define SETT_COMMAND_NAME       QStringLiteral("display_name")
#define SETT_COMMAND_CMD        QStringLiteral("command")
#define SETT_COMMAND_HOTKEY     QStringLiteral("hotkey")
#define SETT_COMMAND_MODE       QStringLiteral("mode")

#define CMD_SEPARATOR           QStringLiteral("__separator__")

#define CMD_PLDR_PATH           "{path}"
#define CMD_PLDR_DIR            "{dir}"
#define CMD_PLDR_FULLNAME       "{fullname}"
#define CMD_PLDR_BASENAME       "{basename}"

#endif // COMMAND_PLACEHOLDERS_H
