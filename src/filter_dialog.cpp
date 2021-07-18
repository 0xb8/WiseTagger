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
#include <QWhatsThis>
#include <QCursor>


FilterDialog::FilterDialog(QWidget * _parent) : QDialog(_parent)
{
	setWindowTitle(tr("Edit queue filter"));
	setMinimumWidth(420);

	setSizeGripEnabled(true);

	QSettings s;
	QFont font(s.value(QStringLiteral("window/font"), QStringLiteral("Consolas")).toString());
	int pixel_size = s.value(QStringLiteral("window/font-size-minmode"), 12).toInt();
	font.setPixelSize(pixel_size);

	m_edit.setMinimumHeight(pixel_size * 2);
	setFixedHeight(pixel_size * 4);
	m_edit.setFont(font);

	auto help_text = tr("Enter one or more tags to filter queue by:"
	                    "<ul><li><b><code>tail</code></b> will match any occurence of <code>tail</code>, e.g. <code>ponytail</code></li></ul>"
	                    "Use quotation marks to match tags exactly:"
	                    "<ul><li><b><code>\"tail\"</code></b> will match just the tag <code>tail</code></li></ul>"
	                    "Use minus sign to exclude tags:"
	                    "<ul><li><b><code>-cat</code></b> will exclude any occurence of <code>cat</code>, e.g. <code>catgirl</code></li>"
	                    "<li><b><code>-\"cat\"</code></b> will exclude just the tag <code>cat</code></li></ul>");
	setWhatsThis(help_text);

	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	QHBoxLayout* l = new QHBoxLayout(this);
	l->addWidget(new QLabel(tr("Filter:"), this));
	l->addWidget(&m_edit);

	auto help_label = new QLabel("<a href=\"#help\">(?)</a>", this);
	l->addWidget(help_label);

	help_label->setOpenExternalLinks(false);
	connect(help_label, &QLabel::linkActivated, this, [help_text, this](auto){
		auto pos = QCursor::pos();
		auto w = qobject_cast<QWidget*>(sender());
		Q_ASSERT(w);
		QWhatsThis::showText(pos, help_text, w);
	});
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


CompletionEdit::CompletionEdit(QWidget * _parent) :
        QLineEdit(_parent),
        m_model(),
        m_completer(&m_model, nullptr)
{

	m_completer.setCompletionRole(Qt::UserRole);
	m_completer.setCompletionMode(QCompleter::PopupCompletion);
	setCompleter(&m_completer);
}

void CompletionEdit::keyPressEvent(QKeyEvent * event)
{
	if(event->key() == Qt::Key_Tab) {
		if((!next_completer())&&(!next_completer()))
			return;
		setText(m_completer.currentCompletion());
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

	m_completer.setCompletionPrefix(text());
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
	m_model.setSourceModel(model);
	if (!model) {
		return;
	}
	m_index = 0;
}

bool CompletionEdit::next_completer()
{
	if(m_completer.setCurrentRow(m_index++))
		return true;
	m_index = 0;
	return false;
}

FilterTagsModel::FilterTagsModel(QObject * parent) : QAbstractListModel(parent)
{

}

void FilterTagsModel::setSourceModel(QAbstractItemModel *tags_model)
{
	beginResetModel();
	m_source_model = tags_model;
	endResetModel();
}

int FilterTagsModel::rowCount(const QModelIndex & parent) const
{
	Q_UNUSED(parent);
	int res =  m_source_model ? m_source_model->rowCount() * 4 : 0;
	return res;
}

QVariant FilterTagsModel::data(const QModelIndex& proxy_index, int role) const
{
	if (!m_source_model || !(role == Qt::DisplayRole || role == Qt::UserRole || role == Qt::TextColorRole))
		return QVariant();

	auto src_index= m_source_model->index(proxy_index.row() % m_source_model->rowCount(), 0);
	Q_ASSERT(m_source_model->checkIndex(src_index));

	if (role == Qt::TextColorRole) {
		return m_source_model->data(src_index, role);
	}

	auto remove_neg = [](const QString& s) {
		if (s.startsWith('-'))
			return s.mid(1);
		return s;
	};

	auto quote = [](const QString& s) {
		return "\"" + s + "\"";
	};

	enum class Kind {
		Normal,
		Quoted,
		Negated,
		QuotedNegated
	};

	auto get_kind = [](auto model, auto index) {
		int kind = index.row() / model->rowCount();
		Q_ASSERT(kind < 4);
		return static_cast<Kind>(kind);
	};

	auto kind = get_kind(m_source_model, proxy_index);

	// normal and negated tags
	if (kind == Kind::Normal || kind == Kind::Negated) {
		auto data = remove_neg(m_source_model->data(src_index, role).toString());

		if (kind ==  Kind::Negated && !data.startsWith('-'))
			data.insert(0, '-');

		return data;
	}

	auto get_tag_quoted = [&remove_neg, &quote](auto model, auto index) {
		return quote(remove_neg(model->data(index, Qt::UserRole).toString()));
	};

	auto get_display_quoted = [&get_tag_quoted](auto model, auto index) {
		auto tag = get_tag_quoted(model, index);
		auto comm = model->data(index, Qt::UserRole+1).toString();
		return comm.isEmpty() ? tag : QStringLiteral("%1  (%2)").arg(tag, comm);
	};

	// quoted tags and negated quoted tags
	if (kind ==  Kind::Quoted || kind == Kind::QuotedNegated) {
		QString data;
		if (role == Qt::UserRole) {
			data = get_tag_quoted(m_source_model, src_index);
		}
		if (role == Qt::DisplayRole) {
			data = get_display_quoted(m_source_model, src_index);
		}

		if (kind == Kind::QuotedNegated)
			data.insert(0, '-');

		return data;
	}
	Q_ASSERT(false && "Unknown kind");
	Q_UNREACHABLE();
}
