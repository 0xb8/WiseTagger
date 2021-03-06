/* Copyright © 2020 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAG_PARSER_H
#define TAG_PARSER_H

/*! \file tag_parser.h
 *  \brief Class @ref TagParser
 */

#include <QStringList>
#include <QVector>
#include <QRegularExpression>
#include <unordered_set>
#include "util/unordered_map_qt.h"


class QTextStream;


/// Stores all tag editing state.
class TagEditState {
public:
	/// Clears edit state.
	void clear();

	/// Check and mark \p tag as already removed.
	bool needRemove(const QString& tag);

	/// Check and mark \p tag as already added.
	bool needAdd(const QString& tag);

	/// Check and mark \p tag as already added related tags.
	bool needAddRelated(const QString& tag);

	/// Check and mark \p tag as already replaced.
	bool needReplace(const QString& tag);

private:

	/// Set of tags that were autoremoved.
	std::unordered_set<QString> m_removed_tags;

	/// Set of tags that were autoadded.
	std::unordered_set<QString> m_added_tags;

	/// Set of tags for which implications have been added.
	std::unordered_set<QString> m_related_tags;

	/// Set of tags that were replaced.
	std::unordered_set<QString> m_replaced_tags;
};


/// Provides a tag file parser and tag fixer.
class TagParser : public QObject {
	Q_OBJECT
public:

	explicit TagParser(QObject* _parent = nullptr);

	/*!
	 * \brief Stores options for fixing tags
	 */
	struct FixOptions {

		/// Enable replacing imageboard tags with shorter version.
		bool replace_imageboard_tags = false;

		/// Enable restoring short version of imagebord tags to original.
		bool restore_imageboard_tags = false;

		/// Enable making author handles first when sorting.
		bool force_author_handle_first = false;

		/// Do tag sorting.
		bool sort = true;

		/// Returns FixOptions from settings.
		static FixOptions from_settings();
	};

	/*!
	 * \brief Fix and sort tags.
	 * \param sort Enable tag sorting.
	 *
	 * Sorts and removes duplicate tags, makes tags lower-case, adds related
	 * tags, replaces tags, autoremoves tags.
	 *
	 * Related/replaced/autoremoved tags are added only once for each
	 * candidate tag to allow user to manually undo changes.
	 */
	QStringList fixTags(TagEditState& state,
	                    QString text,
	                    FixOptions options) const;


	/*!
	 * \brief Get comment for a tag.
	 * \retval Empty string if comment was not found.
	 */
	QString getComment(const QString& tag) const;

	/*!
	 * \brief Get replacement for a tag.
	 * \retval Empty string if replacement was not found.
	 */
	QString getReplacement(const QString& tag) const;

	/*!
	 * \brief Returns list of all known tags.
	 */
	const QStringList& getAllTags() const;

	/*!
	 * \brief  Load and parse tags from \p data.
	 */
	bool loadTagData(const QByteArray& data);

	/*!
	 * \brief Are tag file(s) present.
	 */
	bool hasTagFile() const;

	/*!
	 * \brief Is \p tag autoremoved removed
	 */
	bool isTagRemoved(const QString& tag) const;

signals:

	/*!
	 * \brief Emitted when tag file parse error occured
	 * \param regex_source Regular expression source string
	 * \param error Human-readable parsing error description
	 * \param column Error position in the string
	 */
	void parseError(QString regex_source, QString error, int column);

private:
	/*!
	 * \brief Parse tags and set up necessary internal state.
	 * \param input Text stream with tags.
	 * \return List of top-level tags
	 */
	QStringList parse_tags_file(QTextStream* input);

	/*!
	 * \brief List of tags that are related to specified tag.
	 *
	 * If specified tag was alredy processed by this function, it will not
	 * be processed again to allow user to delete unwanted related tags.
	 */
	QStringList related_tags(TagEditState& state, const QString& tag) const;

	/*!
	 * \brief List of replacement tags for specified tag.
	 *
	 * If there are replacement tags, original tag is removed (cleared).
	 * If specified tag was alredy processed by this function, it will not
	 * be processed again to allow user to restore original tag.
	 */
	QStringList replacement_tags(TagEditState& state, QString& tag) const;

	/*!
	 * \brief Remove \p tag if it was marked as autoremoved.
	 *
	 * If specified tag was already processed by this function, it will not
	 * be processed again to allow user to restore autoremoved tag.
	 */
	void        remove_if_unwanted(TagEditState& state, QString& tag) const;

	//----------------------------------------------------------------------

	/// List of all top-level tags from tag files.
	QStringList m_tags_from_file;

	/// Map of tag comments.
	std::unordered_map<QString,QString> m_comment_tooltips;

	/// Multimap used to keep track of related tags.
	std::unordered_multimap <QString,QString> m_related_tags;

	/// Multimap of replacement tags.
	std::unordered_multimap <QString,QString> m_replaced_tags;

	/// Set of autoremoved tags.
	std::unordered_set      <QString>   m_removed_tags;

	/*!
	 * \brief List of regular expressions to be checked on first tag fix.
	 *
	 * \c .first - regular expression
	 * \c .second - id for related tags search.
	 */
	QVector<QPair<QRegularExpression, QString>> m_regexps;
};
#endif // TAG_PARSER_H
