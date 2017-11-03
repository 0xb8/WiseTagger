/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGINPUT_H
#define TAGINPUT_H

#include <QStringList>
#include <QLineEdit>
#include <QStandardItemModel>
#include <memory>

#include "multicompleter.h"
#include "util/unordered_map_qt.h"

class QKeyEvent;
class QTextStream;


/// The TagInput class provides a Line Edit widget designed for tagging.
class TagInput : public QLineEdit {
	Q_OBJECT
public:

	explicit TagInput(QWidget* _parent = nullptr);

	/**
	 * @brief Fixes and sorts tags.
	 * @param sort Enables tag sorting.
	 *
	 * Sorts and removes duplicate tags, makes tags lower-case, adds related
	 * tags, replaces tags, autoremoves tags.
	 *
	 * Related/replaced/autoremoved tags are added only once for each
	 * candidate tag to allow user to manually undo changes.
	 */
	void fixTags(bool sort = true);

	/**
	 * @brief Loads and parses tags from .txt files.
	 */
	void loadTagFiles(const QStringList& files);

	/**
	 * @brief Resets state used to keep track of already processed tags.
	 */
	void clearTagState();

	/**
	 * @brief Sets initial text.
	 */
	void setText(const QString&);

	/**
	 * @brief Returns imageboard post URL deduced from tags.
	 */
	QString postURL() const;


	/**
	 * @brief Returs whether tag file(s) is present.
	 */
	bool hasTagFile() const;

	/**
	 * @brief Returns tags added by user.
	 * @param exclude_tags_from_file If set to true, only tags not present
	 *        in tag file will be returned.
	 */
	QStringList getAddedTags(bool exclude_tags_from_file = false) const;

	/**
	 * @brief Updates configuration from QSettings
	 */
	void updateSettings();

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	static constexpr int m_minimum_height         = 30;
	static constexpr int m_minimum_height_minmode = 25;


	bool        next_completer();

	/**
	 * @brief Parses tags and sets up necessary internal state.
	 * @param input Text stream with tags.
	 * @return List of top-level tags
	 */
	QStringList parse_tags_file(QTextStream* input);

	/**
	 * @brief Returns list of tags that are related to specified tag.
	 *
	 * If specified tag was alredy processed by this function, it will not
	 * be processed again to allow user to delete unwanted related tags.
	 */
	QStringList related_tags(const QString& tag);

	/**
	 * @brief Returns list of replacement tags for specified tag.
	 *
	 * If there are replacement tags, original tag is removed (cleared).
	 * If specified tag was alredy processed by this function, it will not
	 * be processed again to allow user to restore original tag.
	 */
	QStringList replacement_tags(QString& tag);

	/**
	 * @brief Removes tags if it is marked as autoremoved.
	 *
	 * If specified tag was already processed by this function, it will not
	 * be processed again to allow user to restore autoremoved tag.
	 */
	void        remove_if_unwanted(QString& tag);
	void        updateText(const QString &t);

	int         m_index;
	QStringList m_text_list;
	QStringList m_tags_from_file;
	QString     m_initial_text;
	QString     m_post_url;

	/**
	 * @brief A model used to display completions with comments.
	 *
	 * In order for auto-completion to ignore the comment text, original
	 * tags are inserted into Qt::UserRole, while display tags are inserted
	 * into Qt::DisplayRole.
	 */
	QStandardItemModel m_tags_model;

	/**
	 * @brief Map used to collect tag comments.
	 *
	 * Key is original tag, value is a string with comma-separated comments
	 * (same tag may appear in multiple files with different comments).
	 */
	std::unordered_map<QString,QString> m_comment_tooltips;

	/**
	 * @brief Multimap used to keep track of related tags.
	 *
	 * Key is original tag, value is corresponding related tag.
	 * Special marker value (equal to key) is used to mark tag as processed.
	 */
	std::unordered_multimap <QString,QString> m_related_tags;

	/**
	 * @brief Multimap used to keep track of replacement tags.
	 *
	 * Key is original tag, value is corresponding replacement tag.
	 * Special marker value (equal to key) is used to mark tag as processed.
	 */
	std::unordered_multimap <QString,QString> m_replaced_tags;

	/**
	 * @brief Map used to keep track of autoremoved tags.
	 *
	 * Key is original tag, value is boolean that is set to true if tag was
	 * already processed.
	 */
	std::unordered_map      <QString, bool>   m_removed_tags;
	std::unique_ptr<MultiSelectCompleter>     m_completer;

	using tag_iterator = decltype(std::begin(m_text_list));
};
#endif // TAGINPUT_H
