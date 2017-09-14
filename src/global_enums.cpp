/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "global_enums.h"
#include <type_traits>

/*! \file */

/*!
 * Registers stream operators with Qt MetaType system.
 *
 * Note: must be called BEFORE using QSettings for loading/saving values of the type.
 */
#define REGISTER_METATYPE_STREAM_OPERATORS(E) qRegisterMetaTypeStreamOperators<E>(#E);



/*!
 * Adds implementatation of stream operators for enum type.
 */
#define IMPLEMENT_ENUM_STREAM_OPERATORS(E)                                      \
	QDataStream &operator<<(QDataStream& out, E v) {                        \
		out << static_cast<std::underlying_type_t<E>>(v); return out;   \
	}                                                                       \
	QDataStream &operator>>(QDataStream& in, E & v) {                       \
		using enum_t = std::decay_t<decltype(v)>;                       \
		using repr_t = std::underlying_type_t<enum_t>;                  \
		repr_t val{}; in >> val; v = static_cast<enum_t>(val);          \
		return in;                                                      \
	}

/*!
 * The MetaTypeRegistrator is used to register stream operators with Qt MetaType
 * system before main() execution.
 *
 * Use MetaTypeRegistrator() constructor to register custom stream operators.
 */
struct MetaTypeRegistrator
{
	MetaTypeRegistrator() {
		REGISTER_METATYPE_STREAM_OPERATORS(GlobalEnums::ViewMode)
		REGISTER_METATYPE_STREAM_OPERATORS(GlobalEnums::SortQueueBy)
	}
};
static const MetaTypeRegistrator _;

IMPLEMENT_ENUM_STREAM_OPERATORS(GlobalEnums::ViewMode)
IMPLEMENT_ENUM_STREAM_OPERATORS(GlobalEnums::SortQueueBy)
