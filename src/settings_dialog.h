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

class SettingsDialog : public QDialog
{
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

private:
	void resetModel();

	Q_DISABLE_COPY(SettingsDialog)
	Ui::SettingsDialog *ui;
	QStandardItemModel *m_cmdl;
	QDataWidgetMapper *m_dmpr;

};

class HideColumnsFilter : public QSortFilterProxyModel {
	Q_OBJECT
public:
	explicit HideColumnsFilter(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
	bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
};

class SeparatorDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit SeparatorDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent){}

	QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QString displayText(const QVariant &value, const QLocale &locale) const override;
};

class HotkeyEdit : public QLineEdit
{
	Q_OBJECT
public:
	HotkeyEdit(QWidget *parent = 0);
	~HotkeyEdit();

protected:
	void keyPressEvent(QKeyEvent *);

private:
	QKeySequence m_sequence;
};

#endif
