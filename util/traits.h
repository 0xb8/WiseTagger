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
	template<typename... T>
	struct is_nothrow_swappable_all	{
		static constexpr std::tuple<T...> *t = nullptr;
		enum { value = noexcept(t->swap(*t)) };
	};
	template<typename... T>
	constexpr bool is_nothrow_swappable_all_v = is_nothrow_swappable_all<T...>::value;
}
}
#endif
