/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UNORDERED_MAP_QT_H
#define UNORDERED_MAP_QT_H

#include <unordered_map>
#include <QString>

namespace std {
	/// std::hash specialization to support QString
	template<>
	struct hash<QString> {
		typedef QString      argument_type;
		typedef std::size_t  result_type;
		/// Computes hash of QString
		result_type operator()(const argument_type &s) const {
			using value_t = ushort;
			result_type res{};
			for(auto c : s) {
				res ^= std::hash<value_t>()(c.unicode());
			}
			return std::hash<value_t>()(res);
		}
	};
}

#endif // UNORDERED_MAP_QT_H
