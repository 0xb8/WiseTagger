/* Copyright © 2015 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UTIL_IMAGEBOARD_H
#define UTIL_IMAGEBOARD_H

#include <algorithm>
#include <cctype>
#include <QString>
#include <stdexcept>
#include <util/debug.h>

namespace std {
	template<>
	bool isdigit(QChar c, const locale&) {
		return c.isDigit();
	}
}

namespace ib {

namespace detail {
	using string_t = QString;

	struct imageboard_tag
	{
		string_t	original_name;
		string_t	short_name;
		string_t	long_name;
		string_t	post_url;
	};

	const std::array<imageboard_tag,2> imageboard_tags = {
		imageboard_tag {
			"yande.re",
			"yandere_",
			"yande.re",
			"https://yande.re/post/show/"
		},
		imageboard_tag {
			"Konachan.com",
			"konachan_",
			"Konachan.com -",
			"https://konachan.net/post/show/"
		}
	};

	template<typename T, typename U>
	inline bool starts_with(const T &first, const U &secnd) {
		return std::equal(
			std::begin(secnd), std::end(secnd),
			std::begin(first),
			[](const auto& first_elem, const auto& secnd_elem)
			{
				return first_elem == secnd_elem;
			}
		);
	}

	/* Returns index of board, -1 if not found */
	template<typename T>
	int which_board(const T& t)
	{
		for(size_t i = 0; i < imageboard_tags.size(); ++i) {
			if(t == imageboard_tags[i].original_name)
				return static_cast<int>(i);
			if(starts_with(t, imageboard_tags[i].short_name))
				return static_cast<int>(i);
		}
		pdbg << "not found for" << t;
		return -1;
	}

	/* Extracts id from short version of tag */
	template<typename T>
	T get_id_from_short_tag(const T& tag) {
		T res;

		auto is_digit = [](const auto& c)
		{
			static const std::locale loc;
			return std::isdigit(c, loc);
		};

		auto underscore = std::find(std::begin(tag), std::end(tag), '_');

		if(underscore == std::end(tag) || std::next(underscore) == std::end(tag))
			throw std::invalid_argument("Invalid replacement tag: no underscore / no digits");

		// FIXME: skips non-digits inside short tag id
		std::copy_if(std::next(underscore), std::end(tag), std::back_inserter(res), is_digit);
		return res;
	}
} // namespace detail

/* Finds range of imageboard tags, returns false if imageboard tags not found */
template<typename IteratorT>
bool find_imageboard_tags(IteratorT start, IteratorT end, IteratorT& board_tag, IteratorT& board_id)
{
	if(std::distance(start,end) < 2) {
		return false;
	}

	auto tag_it = std::find_if(start, end, [](const auto& tag)
	{
		for(const auto& i : detail::imageboard_tags)
			if(tag == i.original_name)
				return true;
		return false;
	});

	if(tag_it == end) {
		return false;

	}
	auto id_it = std::find_if(tag_it, end, [](const auto& tag)
	{
		bool ok = false;
		tag.toInt(&ok);
		return ok;
	});

	if(id_it == end) {
		return false;
	}

	if(std::distance(id_it, tag_it) > 2) {
		pdbg << "distance(id,tag) too big";
		return false;
	}

	board_tag = tag_it;
	board_id = id_it;

	return true;
}

/* Replaces imageboard tags with their shorter version. */
template<typename ContainerT>
bool replace_imageboard_tags(ContainerT& cont)
{
	auto post = std::end(cont), id = post;

	if(!find_imageboard_tags(std::begin(cont), std::end(cont),post,id))
		return false;

	int which = detail::which_board(*post);
	Q_ASSERT(which >= 0);

	for(auto i = post; i != id; ++i)
		i->clear();

	id->prepend(detail::imageboard_tags[which].short_name);
	pdbg << "replaced" << *id;
	return true;

}

template<typename IteratorT>
bool find_short_imageboard_tag(IteratorT begin, IteratorT end, IteratorT& res)
{
	for(auto i = begin; i != end; ++i)
		for(const auto& j : detail::imageboard_tags)
			if(std::equal(
				std::begin(j.short_name),
				std::end(j.short_name),
				std::begin(*i)))
			{
				res = i;
				return true;
			}
	res = end;
	return false;

}

/* Restores imageboard tags from their shorter version */
template<typename ContainerT>
bool restore_imageboard_tags(ContainerT& c)
{
	auto begin = std::begin(c);
	auto end   = std::end(c);
	auto short_tag = end;

	if(!find_short_imageboard_tag(begin,end, short_tag))
		return false;

	auto id = detail::get_id_from_short_tag(*short_tag);

	auto which = detail::which_board(*short_tag);
	Q_ASSERT(which >= 0);

	short_tag->clear();
	c.insert(begin, detail::imageboard_tags[which].long_name);

	c.insert(std::next(std::begin(c)), id);
	pdbg << "restored" << *std::begin(c) << *std::next(std::begin(c));

	return true;
}

/* Returns URL of imageboard post */
template<typename ContainerT>
auto get_imageboard_post_url(const ContainerT &c)
{
	const auto begin = std::begin(c);
	const auto end   = std::end(c);

	auto board = std::end(c);
	auto id    = board;
	int which  = -1;

	bool found_long, found_short;
	typename ContainerT::value_type ret;

	if((found_long = find_imageboard_tags(begin, end, board, id))
		|| (found_short = find_short_imageboard_tag(begin, end, board)))
	{
		which = detail::which_board(*board);
		Q_ASSERT(which >= 0);
	}

	if(found_long) {
		ret.append(detail::imageboard_tags[which].post_url);
		return ret.append(*id);
	}

	if(found_short) {
		ret.append(detail::imageboard_tags[which].post_url);
		return ret.append(detail::get_id_from_short_tag(*board));

	}

	return ret;
}

} // namespace imageboard

#endif
