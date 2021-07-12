/* Copyright © 2021 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "filter_dialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>
#include <QSettings>


FilterDialog::FilterDialog(QWidget * _parent) : QDialog(_parent)
{
	setWindowTitle(tr("Edit queue filter"));
	setMinimumWidth(400);

	setSizeGripEnabled(true);

	QSettings s;
	QFont font(s.value(QStringLiteral("window/font"), QStringLiteral("Consolas")).toString());
	int pixel_size = s.value(QStringLiteral("window/font-size-minmode"), 12).toInt();
	font.setPixelSize(pixel_size);

	m_edit.setMinimumHeight(pixel_size * 2);
	setFixedHeight(pixel_size * 4);
	m_edit.setFont(font);

	// hide the question mark button
	auto flags = windowFlags();
	flags = flags & (~Qt::WindowContextHelpButtonHint);
	setWindowFlags(flags);

	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	QHBoxLayout* l = new QHBoxLayout(this);
	l->addWidget(new QLabel(tr("Filter:"), this));
	l->addWidget(&m_edit);
}

void FilterDialog::keyPressEvent(QKeyEvent * event) {
	if(event->key() == Qt::Key_Escape) {
		reject();
		return;
	}

	QDialog::keyPressEvent(event);
}

void FilterDialog::setText(QString text)
{
	m_edit.setText(text);
}

QString FilterDialog::text() const
{
	return m_edit.text();
}

void FilterDialog::setModel(QAbstractItemModel * model)
{
	m_edit.setModel(model);
}


// --------------------------------------------------------


CompletionEdit::CompletionEdit(QWidget * _parent) : QLineEdit(_parent)
{
	m_completer = std::make_unique<MultiSelectCompleter>(QStringList(), nullptr);
	setCompleter(m_completer.get());
}

void CompletionEdit::keyPressEvent(QKeyEvent * event)
{
	if(event->key() == Qt::Key_Tab) {
		if((!next_completer())&&(!next_completer()))
			return;
		setText(m_completer->currentCompletion());
		return;
	}

	if (event->key() == Qt::Key_Exclam) {
		auto t = text();
		int pos = cursorPosition();

		// replace ! by -
		if (t.isEmpty() || t[pos-1].isSpace()) {
			QKeyEvent new_ev(event->type(), Qt::Key_Minus, event->modifiers(), QStringLiteral("-"));
			QLineEdit::keyPressEvent(&new_ev);
			if (new_ev.isAccepted())
				event->accept();
		} else {
			QLineEdit::keyPressEvent(event);
		}
	} else {
		QLineEdit::keyPressEvent(event);
	}

	m_completer->setCompletionPrefix(text());
	m_index = 0;

	if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
		releaseKeyboard();
		auto dialog = qobject_cast<FilterDialog*>(parentWidget());
		Q_ASSERT(dialog);
		dialog->accept();
		return;
	}
}


void CompletionEdit::setModel(QAbstractItemModel * model)
{
	if (model && model == m_prev_model) {
		return;
	}

	m_prev_model = model;
	m_tags_model.clear();

	if (!model) {
		return;
	}

	const auto rows = model->rowCount();
	m_tags_model.setColumnCount(1);
	m_tags_model.setRowCount(2 * rows);

	for(int i = 0; i < rows; ++i) {
		const auto src_index = model->index(i, 0);
		const auto dst_index = m_tags_model.index(i, 0);

		const auto tag = model->data(src_index, Qt::UserRole).toString();
		const auto comm = model->data(src_index, Qt::DisplayRole).toString();

		m_tags_model.setData(dst_index, tag, Qt::UserRole);
		m_tags_model.setData(dst_index, comm, Qt::DisplayRole);

		const auto dst_index_neg = m_tags_model.index(rows + i, 0);
		const auto negtag = tag.startsWith('-') ? tag : "-" + tag;
		const auto negcom = comm.startsWith('-') ? comm : "-" + comm;

		m_tags_model.setData(dst_index_neg, negtag, Qt::UserRole);
		m_tags_model.setData(dst_index_neg, negcom, Qt::DisplayRole);
	}

	m_completer = std::make_unique<MultiSelectCompleter>(&m_tags_model, nullptr);
	m_completer->setCompletionRole(Qt::UserRole);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(m_completer.get());
	m_index = 0;
}

bool CompletionEdit::next_completer()
{
	if(m_completer->setCurrentRow(m_index++))
		return true;
	m_index = 0;
	return false;
}
