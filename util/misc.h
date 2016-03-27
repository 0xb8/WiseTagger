/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include <QByteArray>

namespace util {

QByteArray read_resource_html(const char* filename);

QString duration(std::uint64_t ms);

}
