#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>

namespace Ui {
	class SettingsDialog;
}

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	SettingsDialog(QWidget *parent_ = nullptr);
	~SettingsDialog() override = default;

public slots:
	void reset();
	void apply();
	void restoreDefaults();

signals:
	void updated();

private:
	Q_DISABLE_COPY(SettingsDialog)
	Ui::SettingsDialog *ui;
};

#endif
