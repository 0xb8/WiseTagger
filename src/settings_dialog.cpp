#include "settings_dialog.h"
#include "ui_settings.h"
#include "util/misc.h"
#include <QDirIterator>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QtDebug>

#define S_LOCALE        QStringLiteral("window/language")
#define S_STYLE         QStringLiteral("window/style")
#define S_FONT          QStringLiteral("window/font")
#define S_FONT_SIZE     QStringLiteral("window/font-size")
#define S_SHOW_DIR      QStringLiteral("window/show-current-directory")
#define S_STATISTICS    QStringLiteral("stats/enabled")
#define S_VERCHECK	QStringLiteral("version-check-enabled")
#define S_PROXY_ENABLED QStringLiteral("proxy/enabled")
#define S_PROXY_HOST    QStringLiteral("proxy/host")
#define S_PROXY_PORT    QStringLiteral("proxy/port")
#define S_PROXY_PROTO   QStringLiteral("proxy/protocol")

#define D_LOCALE        QStringLiteral("English")
#define D_STYLE         QStringLiteral("Default")
#define D_FONT          QStringLiteral("Consolas")
#define D_FONT_SIZE     14
#define D_SHOW_DIR      true
#define D_STATISTICS    true
#define D_VERCHECK      true
#define D_PROXY_ENABLED false
#define D_PROXY_HOST    QStringLiteral("")
#define D_PROXY_PORT    9050
#define D_PROXY_PROTO   QStringLiteral("SOCKS5")

SettingsDialog::SettingsDialog(QWidget * parent_) : QDialog(parent_), ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
#ifndef Q_OS_WIN
	ui->vercheckEnabled->hide();
#endif
	connect(ui->dialogButtonBox, &QDialogButtonBox::clicked, [this](QAbstractButton* btn){
		if(qobject_cast<QPushButton*>(btn) == ui->dialogButtonBox->button(QDialogButtonBox::Apply)) {
			apply();
		}
		if(qobject_cast<QPushButton*>(btn) == ui->dialogButtonBox->button(QDialogButtonBox::RestoreDefaults)) {
			restoreDefaults();
		}
	});
	connect(ui->dialogButtonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::apply);
	connect(ui->dialogButtonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reset);
	reset();
}

void SettingsDialog::reset()
{
	QSettings settings;
	QDirIterator it_style(QStringLiteral(":/css"), {"*.css"});
	QDirIterator it_locale(QStringLiteral(":/i18n/"), {"*.qm"});
	auto curr_language = util::language_name(util::language_from_settings());

	ui->appLanguage->clear();
	ui->appStyle->clear();

	while (it_style.hasNext() && !it_style.next().isEmpty()) {
		const auto title = it_style.fileInfo().baseName();
		ui->appStyle->addItem(title);
	}
	while(it_locale.hasNext() && !it_locale.next().isEmpty()) {
		const auto name = it_locale.fileInfo().completeBaseName();
		ui->appLanguage->addItem(name);
	}

	ui->appLanguage->  setCurrentText(curr_language);
	ui->appStyle->     setCurrentText(settings.value(S_STYLE,         D_STYLE).toString());
	ui->fontFamily->   setCurrentText(settings.value(S_FONT,          D_FONT).toString());
	ui->proxyProtocol->setCurrentText(settings.value(S_PROXY_PROTO,   D_PROXY_PROTO).toString());
	ui->showCurrentDir->setChecked(   settings.value(S_SHOW_DIR,      D_SHOW_DIR).toBool());
	ui->statsEnabled-> setChecked(    settings.value(S_STATISTICS,    D_STATISTICS).toBool());
	ui->vercheckEnabled->setChecked(  settings.value(S_VERCHECK,      D_VERCHECK).toBool());
	ui->proxyGroup->   setChecked(    settings.value(S_PROXY_ENABLED, D_PROXY_ENABLED).toBool());
	ui->fontSize->     setValue(      settings.value(S_FONT_SIZE,     D_FONT_SIZE).toUInt());
	ui->proxyPort->    setValue(      settings.value(S_PROXY_PORT,    D_PROXY_PORT).toUInt());
	ui->proxyHost->    setText(       settings.value(S_PROXY_HOST,    D_PROXY_HOST).toString());
}

void SettingsDialog::apply()
{
	QSettings settings;

	auto language_code = util::language_code(ui->appLanguage->currentText());
	auto previous_code = util::language_from_settings();
	if(language_code != previous_code) {
		QMessageBox::information(this,
			tr("Language Changed"),
			tr("<p>Please restart %1 to apply language change.</p>")
				.arg(QStringLiteral(TARGET_PRODUCT)));
	}

	settings.setValue(S_LOCALE,        language_code);
	settings.setValue(S_STYLE,         ui->appStyle->currentText());
	settings.setValue(S_FONT,          ui->fontFamily->currentText());
	settings.setValue(S_FONT_SIZE,     ui->fontSize->value());
	settings.setValue(S_SHOW_DIR,      ui->showCurrentDir->isChecked());
	settings.setValue(S_STATISTICS,    ui->statsEnabled->isChecked());
	settings.setValue(S_VERCHECK,      ui->vercheckEnabled->isChecked());
	settings.setValue(S_PROXY_PROTO,   ui->proxyProtocol->currentText());
	settings.setValue(S_PROXY_PORT,    ui->proxyPort->text());
	settings.setValue(S_PROXY_HOST,    ui->proxyHost->text());
	settings.setValue(S_PROXY_ENABLED, ui->proxyGroup->isChecked());

	emit updated();
}

void SettingsDialog::restoreDefaults()
{
	ui->appLanguage->  setCurrentText(D_LOCALE);
	ui->appStyle->     setCurrentText(D_STYLE);
	ui->fontFamily->   setCurrentText(D_FONT);
	ui->proxyProtocol->setCurrentText(D_PROXY_PROTO);
	ui->showCurrentDir->setChecked(   D_SHOW_DIR);
	ui->statsEnabled-> setChecked(    D_STATISTICS);
	ui->vercheckEnabled->setChecked(  D_VERCHECK);
	ui->proxyGroup->   setChecked(    D_PROXY_ENABLED);
	ui->proxyPort->    setValue(      D_PROXY_PORT);
	ui->fontSize->     setValue(      D_FONT_SIZE);
	ui->proxyHost->    setText(       D_PROXY_HOST);
}
