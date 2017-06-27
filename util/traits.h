/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UTIL_TRAITS_H
#define UTIL_TRAITS_H

namespace util {
namespace traits {
	/// Determines if all of Ts are nothrow-swappable
	template<typename... T>
	class is_nothrow_swappable_all	{
		static constexpr std::tuple<T...> *t = nullptr;
	public:
		enum {
			value = noexcept(t->swap(*t)) ///< \a true if all Ts are nothrow-swappable, \a false othewise
		};
	};
	/// Alias for is_nothrow_swappable_all<>::value
	template<typename... T>
	constexpr bool is_nothrow_swappable_all_v = is_nothrow_swappable_all<T...>::value;
}
}
#endif
