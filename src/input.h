/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef TAGINPUT_H
#define TAGINPUT_H

/*! \file input.h
 *  \brief Class @ref TagInput
 */

#include <QStringList>
#include <QLineEdit>
#include <QStandardItemModel>
#include <memory>

#include "multicompleter.h"
#include "tag_parser.h"


class QKeyEvent;


/// Provides a Line Edit widget designed for tagging.
class TagInput : public QLineEdit {
	Q_OBJECT
public:

	explicit TagInput(QWidget* _parent = nullptr);

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
	void fixTags(bool sort = true);

	/*!
	 * \brief  Load and parse tags from \p data.
	 */
	void loadTagData(const QByteArray& data);

	/*!
	 * \brief Reset state used to keep track of already processed tags.
	 */
	void clearTagState();

	/*!
	 * \brief Set initial text.
	 */
	void setText(const QString&);

	/*!
	 * \brief Set tags while keeping the imageboard id, if present.
	 */
	void setTags(const QString&);

	/*!
	 * \brief Returns tags without imageboard id, if present.
	 */
	QString tags() const;


	/*!
	 * \brief Returns list of tags without imageboard id, if present.
	 */
	QStringList tags_list() const;

	/*!
	 * \brief Returns constant reference to current tag parser.
	 */
	const TagParser& tag_parser() const;


	/*!
	 * \brief Imageboard post URL deduced from tags (might be empty).
	 */
	QString postURL() const;


	/*!
	 * \brief Imageboard post API url deduced from tags (might be empty).
	 */
	QString postTagsApiURL() const;


	/*!
	 * \brief Are tag file(s) present.
	 */
	bool hasTagFile() const;

	/*!
	 * \brief List of tags added by user.
	 * \param exclude_tags_from_file If \c true, only tags not present
	 *        in tag file will be returned.
	 */
	QStringList getAddedTags(bool exclude_tags_from_file = false) const;

	/*!
	 * \brief Update configuration from QSettings
	 */
	void updateSettings();

signals:

	/*!
	 * \brief Emitted when tag file parse error occured
	 * \param regex_source Regular expression source string
	 * \param error Human-readable parsing error description
	 * \param column Error position in the string
	 */
	void parseError(QString regex_source, QString error, int column);


protected:
	void keyPressEvent(QKeyEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;

private:
	static constexpr int m_minimum_height         = 30;
	static constexpr int m_minimum_height_minmode = 25;

	/// Tag file parser and tag fixer
	TagParser   m_tag_parser;

	/// Editing state for current text.
	TagEditState m_edit_state;

	bool        next_completer();

	void        updateText(const QString &t);

	int         m_index;
	QStringList m_text_list;
	QString     m_initial_text;

	/*!
	 * \brief A model used to display completions with comments.
	 *
	 * In order for auto-completion to ignore the comment text, original
	 * tags are inserted into \c Qt::UserRole, while display tags are inserted
	 * into \c Qt::DisplayRole.
	 */
	QStandardItemModel m_tags_model;

	std::unique_ptr<MultiSelectCompleter>     m_completer;
	using tag_iterator = decltype(std::begin(m_text_list));
};
#endif // TAGINPUT_H
