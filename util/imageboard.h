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
#include <stdexcept>
#include <QString>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(iblc, "Imageboard")
#define pdbg qCDebug(iblc)
#define pwarn qCWarning(iblc)

namespace std {
	template<>
	bool isdigit(QChar c, const locale&) {
		return c.isDigit();
	}
}

namespace ib {

namespace detail {
	using string_t = QString;
	#define IB_STRING_LITERAL QStringLiteral

	struct imageboard_tag
	{
		string_t	original_name;
		string_t	short_name;
		string_t	long_name;
		string_t	post_url;
		string_t	post_meta_url;
	};

	const std::array<imageboard_tag,2> imageboard_tags = {
		imageboard_tag {
			IB_STRING_LITERAL("yande.re"),
			IB_STRING_LITERAL("yandere_"),
			IB_STRING_LITERAL("yande.re"),
			IB_STRING_LITERAL("https://yande.re/post/show/%1"),
			IB_STRING_LITERAL("https://yande.re/post.json?tags=id:%1")
		},
		imageboard_tag {
			IB_STRING_LITERAL("Konachan.com"),
			IB_STRING_LITERAL("konachan_"),
			IB_STRING_LITERAL("Konachan.com -"),
			IB_STRING_LITERAL("https://konachan.net/post/show/%1"),
			IB_STRING_LITERAL("https://konachan.net/post.json?tags=id:%1")
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
		board_id = board_tag = end;
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
		board_id = board_tag = end;
		return false;

	}
	auto id_it = std::find_if(tag_it, end, [](const auto& tag)
	{
		bool ok = false;
		tag.toInt(&ok);
		return ok;
	});

	if(id_it == end) {
		board_id = board_tag = end;
		return false;
	}

	if(std::distance(id_it, tag_it) > 2) {
		pdbg << "distance(id,tag) too big";
		board_id = board_tag = end;
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

template<typename StringT>
struct imageboard_meta {
	StringT imageboard_name;
	StringT post_id;
	StringT post_url;
	StringT post_api_url;
};

namespace detail {
	template<typename StringT>
	inline
	imageboard_meta<StringT> make_meta(StringT&& name, StringT&& id, StringT&& url, StringT&& api)
	{
		static_assert(std::is_rvalue_reference<decltype(name)>::value, "rvalue reqired");
		return imageboard_meta<StringT>{name,id,url,api};
	}
}

template<typename IteratorT>
auto get_imageboard_meta(IteratorT ibegin, IteratorT iend)
{
	using val_t = typename IteratorT::value_type;

	auto board = iend, id = iend;
	int which = -1;
	bool found_long, found_short;


	if((found_long = find_imageboard_tags(ibegin, iend, board, id))
		|| (found_short = find_short_imageboard_tag(ibegin, iend, board)))
	{
		which = detail::which_board(*board);
	}

	val_t ibname, ibid, iburl, ibmetaurl;
	if(which >= 0) {
		ibname = detail::imageboard_tags[which].original_name;

		if(found_long) {
			ibid = *id;
		} else if (found_short) {
			ibid = detail::get_id_from_short_tag(*board);
		}

		if(ibid.size() != 0) {
			iburl = detail::imageboard_tags[which].post_url.arg(ibid);
			ibmetaurl = detail::imageboard_tags[which].post_meta_url.arg(ibid);
		}
	}

	return detail::make_meta(std::move(ibname),std::move(ibid),std::move(iburl),std::move(ibmetaurl));
}

} // namespace imageboard

#undef pdbg
#undef pwarn
#endif
