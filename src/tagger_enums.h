/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */


/*! \file Tagger Enums file */
/*! \defgroup Enumerations Public enumeration types */
#ifndef TAGGER_ENUMS_H
#define TAGGER_ENUMS_H

#include <type_traits>
#include <QString>

/*!
 * \brief Result of rename operation.
 * \ingroup Enumerations
 */
enum class RenameStatus : std::int8_t
{
	Cancelled = -1, ///< Rename was cancelled by user.
	Failed = 0,     ///< Rename failed.
	Renamed = 1     ///< Renamed successfully
};


/*!
 * \brief Flags to control UI behavior when renaming file.
 * \ingroup Enumerations
 */
enum class RenameFlags : std::int8_t
{
	Default = 0x0,       ///< Default UI behavior.
	Force = 0x1,         ///< Force rename - do not show rename dialog.
	Uncancelable = 0x2,  ///< Remove \a Cancel button.
};

/*!
 * \brief Set flag bit(s)
 * \ingroup Enumerations
 */
inline RenameFlags operator | (RenameFlags lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	return static_cast<RenameFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

/*!
 * \brief Set flag bit(s)
 * \ingroup Enumerations
 */
inline RenameFlags operator |= (RenameFlags& lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	lhs = static_cast<RenameFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
	return lhs;
}

/*!
 * \brief Test flag bit(s).
 * \ingroup Enumerations
 */
inline bool operator &(RenameFlags lhs, RenameFlags rhs)
{
	using T = std::underlying_type_t<RenameFlags>;
	return static_cast<T>(lhs) & static_cast<T>(rhs);
}

#endif
