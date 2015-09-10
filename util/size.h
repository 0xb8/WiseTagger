/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UTIL_SIZE_H
#define UTIL_SIZE_H

#include <cstddef>
#include <QString>

namespace util {
namespace size {

	template<class T, std::size_t N>
	constexpr std::size_t array_size(T (&)[N]) noexcept { return N; }

	inline std::size_t to_kib(std::size_t bytes)
	{
		return bytes / 1024;
	}

	inline double to_mib(std::size_t bytes)
	{
		return static_cast<double>(bytes) / 1048576.0;
	}

	inline QString printable(std::size_t bytes)
	{
		return bytes < 1024*1024 // if less than 1 Mb show exact KB size
			? QString("%1Kb").arg(to_kib(bytes))
			: QString("%1Mb").arg(to_mib(bytes), 0,'f', 3);
	}
}
}

#endif
