/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef GLOBAL_ENUMS_H
#define GLOBAL_ENUMS_H

#include <QObject>
#include <QDataStream>


/** \dir src
 *  \brief Core sources.
 */

/*! 
 * \file global_enums.h
 * \brief Class \ref GlobalEnums 
 */

/*!
 * Declares stream operators used to save and load values of custom enum types with QSettings etc.
 */
#define ENUM_STREAM_OPERATORS(E)                                                \
	QDataStream& operator<<(QDataStream&, E);                               \
	QDataStream& operator>>(QDataStream&, E&);


/*!
 * \brief GlobalEnums class used to enable Qt Meta Type support for enum types.
 *
 * This class cannot be instantiated. 
 * To register enum type with Qt MetaType, add Q_ENUM(YourEnum) directly after 
 * YourEnum definition. 
 *
 * Then add ENUM_STREAM_OPERATORS(GlobalEnums::YourEnum) after this class to 
 * enable saving and loading values of YourEnum with QSettings.
 * After that, proceed to file global_enums.cpp and register declared stream 
 * operators with Qt MetaType system.
 */
class GlobalEnums
{
	Q_GADGET
public:
	/// Window view mode.
	enum class ViewMode
	{
		Normal,            ///< Show in default mode.
		Minimal            ///< Show in minimal mode.
	};
	Q_ENUM(ViewMode)

	/// File queue sorting criteria.
	enum class SortQueueBy
	{
		FileName,          ///< Sort by file name only.
		FileType,          ///< Sort by file type, then by file name.
		FileSize,          ///< Sort by file size, then by file name.
		ModificationDate,  ///< Sort by modification date, then by file name.
		FileNameLength,    ///< Sort by file name length, then by file name.
		TagCount,          ///< Sort by tag count, then by file name.
	};
	Q_ENUM(SortQueueBy)

	/*!
	 * \brief Specifies input editing mode.
	 */
	enum class EditMode {
		Tagging, ///< In this mode tags are sorted and deduplicated when pressing Enter or saving file.
		Naming   ///< In this mode entered text is not modified at all.
	};
	Q_ENUM(EditMode);

	/*!
	 * \brief Returns next edit mode for \p current mode.
	 */
	static EditMode next_edit_mode(EditMode current);


	/*!
	 * \brief Specifies command output mode.
	 */
	enum class CommandOutputMode {
		Ignore,  ///< Command output is ignored, and the process is detached.
		Replace, ///< Command output replaces current tags
		Append,  ///< Command output appended to current tags
		Prepend, ///< Command output prepended to current tags
		Copy,    ///< Command output is copied into the system clipboard
		Show     ///< Command output is displayed as a notification message
	};
	Q_ENUM(CommandOutputMode);

	GlobalEnums() = delete;
};
ENUM_STREAM_OPERATORS(GlobalEnums::ViewMode)
ENUM_STREAM_OPERATORS(GlobalEnums::SortQueueBy)
ENUM_STREAM_OPERATORS(GlobalEnums::EditMode)
ENUM_STREAM_OPERATORS(GlobalEnums::CommandOutputMode)


/// Alias for \ref GlobalEnums::ViewMode enumeration
using ViewMode    = GlobalEnums::ViewMode;

/// Alias for \ref GlobalEnums::SortQueueBy enumeration
using SortQueueBy = GlobalEnums::SortQueueBy;

/// Alias for \ref GlobalEnums::EditMode enumeration
using EditMode = GlobalEnums::EditMode;

/// Alias for \ref GlobalEnums::CommandOutputMode enumeration
using CommandOutputMode = GlobalEnums::CommandOutputMode;

#endif // GLOBAL_ENUMS_H
