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
#include <QStandardItemModel>
#include <memory>

#include "multicompleter.h"


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
	std::unique_ptr<MultiSelectCompleter> m_completer;
	QStandardItemModel m_tags_model;
	QAbstractItemModel* m_prev_model = nullptr;
	int m_index = 0;

	bool next_completer();
};



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
