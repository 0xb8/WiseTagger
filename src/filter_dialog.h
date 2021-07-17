/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef FILTER_DIALOG_H
#define FILTER_DIALOG_H

/*! \file filter_dialog.h
 *  \brief Class @ref FilterDialog
 */

#include <QDialog>
#include <QLineEdit>
#include <QAbstractListModel>
#include "multicompleter.h"


/*!
 * \brief Filter tags autocomplete model.
 *
 * Acts as a proxy for actual tag autocomplete model.
 * Each tag is presented in 4 variations: normal, negated, quoted, negated quoted.
 */
class FilterTagsModel : public QAbstractListModel
{
	Q_OBJECT
public:
	explicit FilterTagsModel(QObject* parent = nullptr);
	~FilterTagsModel() = default;

	/*!
	 * \brief Sets source tags model.
	 */
	void setSourceModel(QAbstractItemModel* tags_model);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
	// source model
	QAbstractItemModel* m_source_model = nullptr;
};


/*!
 * \brief Line edit widget with tag filter autocomplete.
 */
class CompletionEdit : public QLineEdit {
	Q_OBJECT;
public:
	CompletionEdit(QWidget* _parent=nullptr);

	/*!
	 * \brief Set tag completion model.
	 */
	void setModel(QAbstractItemModel* model);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	// proxy model for filter autocomplete
	FilterTagsModel m_model;

	// filter completer
	MultiSelectCompleter m_completer;
	int m_index = 0;
	bool next_completer();
};



/*!
 * \brief Dialog widget for editing the queue filter.
 */
class FilterDialog : public QDialog {
	Q_OBJECT
public:

	FilterDialog(QWidget* _parent=nullptr);

	/*!
	 * \brief Set current filter text.
	 */
	void setText(QString text);

	/*!
	 * \brief Returns current filter text.
	 */
	QString text() const;

	/*!
	 * \brief Set tag completion model.
	 */
	void setModel(QAbstractItemModel* model);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	CompletionEdit m_edit;

};
#endif // FILTER_DIALOG_H
