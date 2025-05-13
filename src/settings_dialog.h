/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include <QLineEdit>

namespace Ui {
	class SettingsDialog;
}

class QStandardItemModel;
class QDataWidgetMapper;
class QKeyEvent;
class ImageboardFetchEnabledModel;

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(QWidget *parent_ = nullptr);
	~SettingsDialog() override;

public slots:
	void reset();
	void apply();
	void restoreDefaults();

signals:
	void updated();
	void restart();

private:
	void resetModel();

	Q_DISABLE_COPY(SettingsDialog)
	Ui::SettingsDialog *ui;
	QStandardItemModel *m_cmdl;
	QDataWidgetMapper *m_dmpr;
	ImageboardFetchEnabledModel* m_ib_fetch_enabled_mdl;

};


class HideColumnsFilter : public QSortFilterProxyModel {
	Q_OBJECT
public:
	explicit HideColumnsFilter(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
	bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
};


class SeparatorDelegate : public QStyledItemDelegate {
	Q_OBJECT
public:
	explicit SeparatorDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent){}

	QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QString displayText(const QVariant &value, const QLocale &locale) const override;
};


class HotkeyEdit : public QLineEdit {
	Q_OBJECT
public:
	HotkeyEdit(QWidget *parent = 0);
	~HotkeyEdit();

protected:
	void keyPressEvent(QKeyEvent *);

private:
	QKeySequence m_sequence;
};


class ImageboardFetchEnabledModel : public QAbstractTableModel {
	Q_OBJECT

	enum ImageboardSettingsKeyEnum {
		ImageboardSettingsKeyRole = Qt::UserRole + 1
	};

	enum ImageBoardIndex {
		Yandere = 0,
		Konachan,
		Sankaku,
		Danbooru,
		Gelbooru,

		_imageboard_count // must be last!
	};

	bool m_state[_imageboard_count];
public:
	ImageboardFetchEnabledModel();

	void reset();
	void commit();
	void restoreDefault();

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex &index, int role) const override;;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
};

#endif
