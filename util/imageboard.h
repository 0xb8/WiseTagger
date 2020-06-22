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
#include <array>
#include <QString>

#ifndef pdbg
#include <QtDebug>
#define pdbg qDebug()
#define pwarn qWarning()
#define IMAGEBOARD_LOG_DEFINES
#endif

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

	const std::array<imageboard_tag,10> imageboard_tags = {
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
		},
		imageboard_tag {
			IB_STRING_LITERAL("dbru.com"), // 2010 suggestion, not implemented
			IB_STRING_LITERAL("danbooru_"),
			IB_STRING_LITERAL("danbooru.donmai.us"),
			IB_STRING_LITERAL("https://danbooru.donmai.us/posts/%1"),
			IB_STRING_LITERAL("https://danbooru.donmai.us/posts/%1.json") // FIXME: somewhy returns JSON-object with "tag_string" on browser yet replies nothing to WiseTagger
		},
		imageboard_tag {
			IB_STRING_LITERAL("drawfriends"),
			IB_STRING_LITERAL("drawfriends_"),
			IB_STRING_LITERAL("drawfriends.booru.org"),
			IB_STRING_LITERAL("https://drawfriends.booru.org/index.php?page=post&s=view&id=%1"),
			IB_STRING_LITERAL("") // FIXME: dirty stub, drawfiends have API disabled
		},
		imageboard_tag {
			IB_STRING_LITERAL("pixiv"),
			IB_STRING_LITERAL("pixiv_"),
			IB_STRING_LITERAL("pixiv.net"),
			IB_STRING_LITERAL("https://www.pixiv.net/artworks/%1"),
			IB_STRING_LITERAL("https://public-api.secure.pixiv.net/v1/works/%1.json") // TODO: requires oauth, so this is just a stub - see https://github.com/tombfix/core/commit/4a00148 & https://danbooru.donmai.us/wiki_pages/help:pixiv_api
		},
		imageboard_tag {
			IB_STRING_LITERAL("deviantart"),
			IB_STRING_LITERAL("deviantart_"),
			IB_STRING_LITERAL("deviantart.com"),
			IB_STRING_LITERAL("https://fav.me/%1"), //fav.me is usually used with shortened alphanumeric urls (ex. ddow5qh → 827871497), but strangely, decoded ones are accepted too. In any case, https://www.deviantart.com/view/%1 would deal with numeric
			IB_STRING_LITERAL("https://www.deviantart.com/api/v1/oauth2/deviation/metadata?deviationids%5B%5D=4A30FF08-3703-3034-C875-C901845E6184&ext_submission=false&ext_camera=false&ext_stats=false&ext_collection=false&mature_content=true")
			/* TODO: To connect to this endpoint, you need an Oauth2 Access Token - see https://www.deviantart.com/developers/authentication
				%1 (deviation_id) won't work here - one needs to know UUID - see https://stackoverflow.com/questions/28581350/obtain-deviantart-deviation-id-uuid-from-page-url
				Currently shown 4A30FF08-3703-3034-C875-C901845E6184 is a stub - ID 827871497. On success the endpoint provides JSON object metadata with sub-array tags
			*/
		},
		imageboard_tag {
			IB_STRING_LITERAL("artstation"),
			IB_STRING_LITERAL("artstation_"),
			IB_STRING_LITERAL("artstation.com"),
			IB_STRING_LITERAL("https://www.artstation.com/artwork/%1"), //FIXME: artstation uniqueIDs are case-sensitive alphanumeric, probably won't work with current implementation
			IB_STRING_LITERAL("")
		},
		imageboard_tag {
			IB_STRING_LITERAL("hentai-foundry"),
			IB_STRING_LITERAL("hfoundry_"),
			IB_STRING_LITERAL("hentai-foundry.com"),
			IB_STRING_LITERAL("https://www.hentai-foundry.com/pictures/%1"),
			IB_STRING_LITERAL("")
		},
		imageboard_tag {
			IB_STRING_LITERAL("medicalwhiskey"),
			IB_STRING_LITERAL("medicalwhiskey_"),
			IB_STRING_LITERAL("medicalwhiskey.com"),
			IB_STRING_LITERAL("http://medicalwhiskey.com/?p=%1"),
			IB_STRING_LITERAL("")
		},
		imageboard_tag {
			IB_STRING_LITERAL("vidyart"),
			IB_STRING_LITERAL("vidyart_"),
			IB_STRING_LITERAL("vidyart.booru.org"),
			IB_STRING_LITERAL("https://vidyart.booru.org/index.php?page=post&s=view&id=%1"),
			IB_STRING_LITERAL("")
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

		auto is_digit = [](QChar c) {
			return c.isDigit();
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

#ifdef IMAGEBOARD_LOG_DEFINES
#undef pdbg
#undef pwarn
#endif // IMAGEBOARD_LOG_DEFINES

#endif // UTIL_IMAGEBOARD_H
