/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGGER_ENUMS_H
#define TAGGER_ENUMS_H

#include <type_traits>
#include <QString>

enum class RenameStatus : std::int8_t
{
	Cancelled = -1,
	Failed = 0,
	Renamed = 1
};

enum class RenameFlags : std::int8_t
{
	Default = 0x0,
	Force = 0x1,
	Uncancelable = 0x2,
};

inline RenameFlags operator | (RenameFlags lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	return static_cast<RenameFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline RenameFlags operator |= (RenameFlags& lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	lhs = static_cast<RenameFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
	return lhs;
}

inline bool operator &(RenameFlags lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	return static_cast<T>(lhs) & static_cast<T>(rhs);
}

#endif
