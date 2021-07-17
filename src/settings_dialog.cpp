/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "settings_dialog.h"
#include "ui_settings.h"
#include "util/misc.h"
#include "util/project_info.h"
#include "util/command_placeholders.h"
#include <QApplication>
#include <QDataWidgetMapper>
#include <QDirIterator>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QtDebug>
#include <QThread>

#define S_LOCALE        QStringLiteral("window/language")
#define S_STYLE         QStringLiteral("window/style")
#define S_FONT          QStringLiteral("window/font")
#define S_FONT_SIZE     QStringLiteral("window/font-size")
#define S_SHOW_DIR      QStringLiteral("window/show-current-directory")
#define S_STATISTICS    QStringLiteral("stats/enabled")
#define S_VERCHECK      QStringLiteral("version-check-enabled")
#define S_TRACK_TAGS    QStringLiteral("track-added-tags")
#define S_PROXY_ENABLED QStringLiteral("proxy/enabled")
#define S_PROXY_USE_SYS QStringLiteral("proxy/use_system")
#define S_PROXY_HOST    QStringLiteral("proxy/host")
#define S_PROXY_PORT    QStringLiteral("proxy/port")
#define S_PROXY_PROTO   QStringLiteral("proxy/protocol")
#define S_PRELOAD       QStringLiteral("performance/pixmap_precache_enabled")
#define S_PRELOAD_CNT   QStringLiteral("performance/pixmap_precache_count")
#define S_PRELOAD_MEM   QStringLiteral("performance/pixmap_cache_size")

#define D_LOCALE        QStringLiteral("English")
#define D_STYLE         QStringLiteral("Default")
#define D_FONT          QStringLiteral("Consolas")
#define D_FONT_SIZE     14
#define D_SHOW_DIR      true
#define D_STATISTICS    true
#define D_VERCHECK      true
#define D_TRACK_TAGS    true
#define D_PROXY_ENABLED false
#define D_PROXY_USE_SYS false
#define D_PROXY_HOST    QStringLiteral("")
#define D_PROXY_PORT    9050
#define D_PROXY_PROTO   QStringLiteral("SOCKS5")
#define D_PRELOAD       true
#define D_PRELOAD_CNT   std::max(QThread::idealThreadCount() / 2, 1)
#define D_PRELOAD_MEM   64

#ifdef Q_OS_WIN
	#define EXECUTABLE_EXTENSION SettingsDialog::tr("Executable Files (*.exe)")
#else
	#define EXECUTABLE_EXTENSION QStringLiteral("")
#endif

namespace logging_category {
	Q_LOGGING_CATEGORY(settdialog, "SettingsDialog")
}
#define pdbg qCDebug(logging_category::settdialog)
#define pwarn qCWarning(logging_category::settdialog)


SettingsDialog::SettingsDialog(QWidget * parent_) : QDialog(parent_), ui(new Ui::SettingsDialog), m_cmdl(nullptr), m_dmpr(nullptr)
{
	ui->setupUi(this);
#ifndef Q_OS_WIN
	ui->vercheckEnabled->hide();
#endif

	connect(ui->dialogButtonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::apply);
	connect(ui->dialogButtonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reset);
	connect(ui->dialogButtonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* btn){
		if(qobject_cast<QPushButton*>(btn) == ui->dialogButtonBox->button(QDialogButtonBox::Apply)) {
			apply();
		}
		if(qobject_cast<QPushButton*>(btn) == ui->dialogButtonBox->button(QDialogButtonBox::RestoreDefaults)) {
			restoreDefaults();
		}
	});
	connect(ui->useSystemProxy, &QCheckBox::toggled, this, [this](bool toggled)
	{
		ui->proxyHost->setDisabled(toggled);
		ui->proxyPort->setDisabled(toggled);
		ui->proxyProtocol->setDisabled(toggled);
	});
	connect(ui->exportSettingsBtn, &QPushButton::clicked, this, [this](){
		QSettings settings;
		auto portable = settings.value(QStringLiteral("settings-portable"), false).toBool();
		auto path = QFileDialog::getSaveFileName(this,
			tr("Export settings to file"),
			portable
				? qApp->applicationDirPath()+QStringLiteral("/settings/catgirl/WiseTagger.ini")
				: QDir::homePath()+QStringLiteral("/WiseTagger.ini"),
			tr("Settings Files (*.ini)"));

		if(path.isEmpty()) {
			return;
		}

		if(util::backup_settings_to_file(path)) {
			QMessageBox::information(this, tr("Export success"),
				tr("Successfully exported settings!"));
		} else {
			QMessageBox::critical(this, tr("Export failed"),
				tr("Could not export settings to file <b>%1</b>. Check directory permissions and try again.").arg(path));
		}

	});
	connect(ui->importSettingsBtn, &QPushButton::clicked, this, [this](){
		auto path = QFileDialog::getOpenFileName(this,
			tr("Import settings from file"), QDir::homePath()+QStringLiteral("/WiseTagger.ini"),
			tr("Settings Files (*.ini)"));
		if(path.isEmpty()) {
			return;
		}

		if(util::restore_settings_from_file(path)) {
			QMessageBox::information(this, tr("Import success"),
				tr("Successfully imported settings!"));
		} else {
			QMessageBox::critical(this, tr("Export failed"),
				tr("Could not import settings from file <b>%1</b>. File may be corrupt or no read permissions.").arg(path));
		}

	});
	connect(ui->cmdExecutableBrowse, &QPushButton::clicked, this, [this](){
		auto dir = QFileInfo(ui->cmdExecutableEdit->text()).absolutePath();
		auto n = QFileDialog::getOpenFileName(this,tr("Select Executable"),dir, EXECUTABLE_EXTENSION);
		if(!n.isEmpty()) {
			ui->cmdExecutableEdit->setText(n);
			m_dmpr->submit(); // NOTE: it's not doing it automatically for some reason
		}
	});
	// Placeholder inserters
	connect(ui->p_filepath, &QAction::triggered, this, [this](bool){
		ui->cmdArgsEdit->insert(QStringLiteral(" " CMD_PLDR_PATH " "));
	});
	connect(ui->p_filedir, &QAction::triggered, this, [this](bool){
		ui->cmdArgsEdit->insert(QStringLiteral(" " CMD_PLDR_DIR " "));
	});
	connect(ui->p_filename, &QAction::triggered, this, [this](bool){
		ui->cmdArgsEdit->insert(QStringLiteral(" " CMD_PLDR_FULLNAME " "));
	});
	connect(ui->p_basename, &QAction::triggered, this, [this](bool){
		ui->cmdArgsEdit->insert(QStringLiteral(" " CMD_PLDR_BASENAME " "));
	});
	// Move down
	connect(ui->downCmd, &QPushButton::clicked, this, [this](){
		auto row = ui->cmdView->currentIndex().row();
		if(row < ui->cmdView->model()->rowCount()-1) {
			auto list = m_cmdl->takeRow(row);
			m_cmdl->insertRow(row+1, list);
			ui->cmdView->selectRow(row+1);
		}
		ui->cmdView->setFocus();

	});
	// Move up
	connect(ui->upCmd, &QPushButton::clicked, this, [this](){
		auto row = ui->cmdView->currentIndex().row();
		if(row > 0) {
			auto list = m_cmdl->takeRow(row);
			m_cmdl->insertRow(row-1, list);
			ui->cmdView->selectRow(row-1);
		}
		ui->cmdView->setFocus();
	});
	// Add new command
	connect(ui->a_newcmd, &QAction::triggered, this, [this](bool){
		auto row = ui->cmdView->currentIndex().row();
		auto new_row = row + 1;
		m_cmdl->insertRow(new_row, QList<QStandardItem*>());
		m_cmdl->setData(m_cmdl->index(new_row, 3), CMD_PLDR_PATH);
		ui->cmdView->selectRow(new_row);
		ui->cmdView->setFocus();
		ui->cmdView->edit(ui->cmdView->model()->index(new_row,0));

	});
	// Add new separator
	connect(ui->a_newsep, &QAction::triggered, this, [this](bool){
		auto row = ui->cmdView->currentIndex().row();
		m_cmdl->insertRow(row+1, QList<QStandardItem*>{new QStandardItem(CMD_SEPARATOR)});
		ui->cmdView->selectRow(row+1);
		ui->cmdView->setFocus();
	});
	// Remove button
	connect(ui->removeCmd, &QPushButton::clicked, this, [this](){
		ui->a_removeitem->setData(ui->cmdView->currentIndex());
		ui->a_removeitem->trigger();
	});
	// Remove action
	connect(ui->a_removeitem, &QAction::triggered, this, [this](bool){
		if(!ui->a_removeitem->data().isNull()) {
			auto index = ui->a_removeitem->data().toModelIndex();
			if(!index.isValid()) return;
			auto row = index.row();
			auto cmd = m_cmdl->data(m_cmdl->index(row, 0)).toString();
			int reply = -1;
			if(cmd != CMD_SEPARATOR) {
				if(cmd.isEmpty()) cmd = tr("Unknown command");
				reply = QMessageBox::question(this,
					tr("Remove command?"),
					tr("<p>Remove <b>%1</b>?</p><p>This action cannot be undone!</p>").arg(cmd),
					QMessageBox::Yes, QMessageBox::No);
			}
			if(reply == -1 || reply == QMessageBox::Yes) {
				m_cmdl->removeRow(row);
			}
		}
	});

	// Setup context menu for table view
	auto menu_ctx = new QMenu(this);
	menu_ctx->addAction(ui->a_newcmd);
	menu_ctx->addAction(ui->a_newsep);
	menu_ctx->addSeparator();
	menu_ctx->addAction(ui->a_removeitem);

	connect(ui->cmdView, &QTableView::customContextMenuRequested, this, [this, menu_ctx](const QPoint& p) {
		auto index = ui->cmdView->indexAt(p);
		ui->a_removeitem->setData(index);
		menu_ctx->exec(ui->cmdView->mapToGlobal(p));
	});

	// Setup menu for add command button
	auto menu_acts = new QMenu(this);
	menu_acts->addAction(ui->a_newcmd);
	menu_acts->addAction(ui->a_newsep);

	// Setup menu for placeholders button
	auto menu_ph = new QMenu(this);
	menu_ph->addAction(ui->p_filepath);
	menu_ph->addAction(ui->p_filedir);
	menu_ph->addAction(ui->p_filename);
	menu_ph->addAction(ui->p_basename);

	// Attach menus
	ui->newCmd->setMenu(menu_acts);
	ui->cmdArgsPlaceholderShow->setMenu(menu_ph);
	ui->cmdView->setContextMenuPolicy(Qt::CustomContextMenu);
	reset();
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
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
	ui->trackAddedTags->setChecked(   settings.value(S_TRACK_TAGS,    D_TRACK_TAGS).toBool());
	ui->proxyGroup->   setChecked(    settings.value(S_PROXY_ENABLED, D_PROXY_ENABLED).toBool());
	ui->useSystemProxy->setChecked(   settings.value(S_PROXY_USE_SYS, D_PROXY_USE_SYS).toBool());
	ui->fontSize->     setValue(      settings.value(S_FONT_SIZE,     D_FONT_SIZE).toUInt());
	ui->proxyPort->    setValue(      settings.value(S_PROXY_PORT,    D_PROXY_PORT).toUInt());
	ui->proxyHost->    setText(       settings.value(S_PROXY_HOST,    D_PROXY_HOST).toString());
	ui->preloadGroup-> setChecked(    settings.value(S_PRELOAD,       D_PRELOAD).toBool());
	ui->preloadCount-> setValue(      settings.value(S_PRELOAD_CNT,   D_PRELOAD_CNT).toUInt());
	ui->cacheMemory->  setValue(      settings.value(S_PRELOAD_MEM,   D_PRELOAD_MEM).toUInt());

	resetModel();

	delete m_dmpr;
	m_dmpr = new QDataWidgetMapper(this);
	m_dmpr->setModel(m_cmdl);
	m_dmpr->addMapping(ui->cmdExecutableEdit, 2);
	m_dmpr->addMapping(ui->cmdArgsEdit, 3);
	m_dmpr->toFirst();

	auto fmd = new HideColumnsFilter(m_cmdl);
	fmd->setSourceModel(m_cmdl);
	ui->cmdView->setModel(fmd);
	ui->cmdView->setItemDelegate(new SeparatorDelegate(m_cmdl));
	ui->cmdView->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);

	auto disable_widgets = [this](bool val)
	{
		ui->cmdExecutableEdit->setDisabled(val);
		ui->cmdExecutableBrowse->setDisabled(val);
		ui->cmdArgsEdit->setDisabled(val);
		ui->cmdArgsPlaceholderShow->setDisabled(val);
	};

	connect(ui->cmdView->selectionModel(), &QItemSelectionModel::currentRowChanged,
	this, [this, disable_widgets](auto a, auto){
		m_dmpr->setCurrentIndex(a.row());
		bool disabled = false;
		if(m_cmdl->data(m_cmdl->index(a.row(), 0)) == CMD_SEPARATOR) {
			disabled = true;
		}
		disable_widgets(disabled);
	});
}

void SettingsDialog::resetModel()
{
	auto join_args = [](const QStringList& args) {
		QString r;
		for(auto arg : args) {
			if(arg.indexOf(' ') != -1) {
				arg.prepend('"');
				arg.append('"');
			}
			r.append(arg);
			r.append(" ");
		}
		return r;
	};

	QSettings st;
	auto size = st.beginReadArray(SETT_COMMANDS_KEY);

	delete m_cmdl;
	m_cmdl = new QStandardItemModel(size, 4, this);

	m_cmdl->setHeaderData(0, Qt::Horizontal,tr("Command Name"));
	m_cmdl->setHeaderData(1, Qt::Horizontal,tr("Hotkey"));

	for(int i = 0; i < size; ++i) {
		st.setArrayIndex(i);
		auto path = st.value(SETT_COMMAND_CMD).toStringList();

		m_cmdl->setItem(i, 0, new QStandardItem(st.value(SETT_COMMAND_NAME).toString()));
		m_cmdl->setItem(i, 1, new QStandardItem(st.value(SETT_COMMAND_HOTKEY).toString()));
		if(!path.isEmpty())
			m_cmdl->setItem(i,2, new QStandardItem(path.first()));

		if(path.size() > 1) {
			path.removeFirst();
			m_cmdl->setItem(i, 3, new QStandardItem(join_args(path)));
		}
	}
	st.endArray();
}

void SettingsDialog::apply()
{
	QSettings settings;

	auto language_code = util::language_code(ui->appLanguage->currentText());
	auto previous_code = util::language_from_settings();
	bool lang_changed = language_code != previous_code;
	bool style_changed = ui->appStyle->currentText() != settings.value(S_STYLE).toString();

	settings.setValue(S_LOCALE,        language_code);
	settings.setValue(S_STYLE,         ui->appStyle->currentText());
	settings.setValue(S_FONT,          ui->fontFamily->currentText());
	settings.setValue(S_FONT_SIZE,     ui->fontSize->value());
	settings.setValue(S_SHOW_DIR,      ui->showCurrentDir->isChecked());
	settings.setValue(S_STATISTICS,    ui->statsEnabled->isChecked());
	settings.setValue(S_VERCHECK,      ui->vercheckEnabled->isChecked());
	settings.setValue(S_TRACK_TAGS,    ui->trackAddedTags->isChecked());
	settings.setValue(S_PROXY_PROTO,   ui->proxyProtocol->currentText());
	settings.setValue(S_PROXY_PORT,    ui->proxyPort->text());
	settings.setValue(S_PROXY_HOST,    ui->proxyHost->text());
	settings.setValue(S_PROXY_ENABLED, ui->proxyGroup->isChecked());
	settings.setValue(S_PROXY_USE_SYS, ui->useSystemProxy->isChecked());
	settings.setValue(S_PRELOAD,       ui->preloadGroup->isChecked());
	settings.setValue(S_PRELOAD_CNT,   ui->preloadCount->value());
	settings.setValue(S_PRELOAD_MEM,   ui->cacheMemory->value());

	auto parse_arguments = [](const QString & args)
	{
		QStringList ret;
		QString curr_arg;

		bool open_quote = false;
		for(auto c : qAsConst(args)) {
			if(c == '"') {
				open_quote = !open_quote;
				continue;
			}
			if(c.isSpace() && !open_quote) {
				if(!curr_arg.isEmpty()) {
					ret.push_back(curr_arg);
					curr_arg.clear();
				}
				continue;
			}
			curr_arg.push_back(c);
		}
		if(!curr_arg.isEmpty()) {
			ret.push_back(curr_arg);
		}
		return ret;
	};

	int invalid_num = 0;
	settings.beginWriteArray(SETT_COMMANDS_KEY);

	for(int i = 0; i < m_cmdl->rowCount(); ++i) {
		settings.setArrayIndex(i);
		auto name = m_cmdl->data(m_cmdl->index(i,0)).toString();
		if(name.isEmpty())
			name = tr("Unknown command %1").arg(++invalid_num);

		auto hotkey = m_cmdl->data(m_cmdl->index(i,1)).toString();
		auto exec_path = m_cmdl->data(m_cmdl->index(i,2)).toString();
		auto args = parse_arguments(m_cmdl->data(m_cmdl->index(i,3)).toString());
		args.prepend(exec_path);

		settings.setValue(SETT_COMMAND_NAME, name);
		settings.setValue(SETT_COMMAND_HOTKEY, hotkey);
		settings.setValue(SETT_COMMAND_CMD, args);
	}
	settings.endArray();
	emit updated();

	if (lang_changed || style_changed) {
		emit restart();
	}
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
	ui->trackAddedTags->setChecked(   D_TRACK_TAGS);
	ui->proxyGroup->   setChecked(    D_PROXY_ENABLED);
	ui->useSystemProxy->setChecked(   D_PROXY_USE_SYS);
	ui->preloadGroup-> setChecked(    D_PRELOAD);
	ui->proxyHost->    setText(       D_PROXY_HOST);
	ui->proxyPort->    setValue(      D_PROXY_PORT);
	ui->fontSize->     setValue(      D_FONT_SIZE);
	ui->preloadCount-> setValue(      D_PRELOAD_CNT);
	ui->cacheMemory->  setValue(      D_PRELOAD_MEM);
}

bool HideColumnsFilter::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
	if(source_column > 1) return false;
	return QSortFilterProxyModel::filterAcceptsColumn(source_column, source_parent);
}

QWidget* SeparatorDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if(index.sibling(index.row(), 0).data() == CMD_SEPARATOR) {
		return nullptr;
	}
	if(index.column() == 1) {
		auto editor = new HotkeyEdit(parent);
		return editor;
	}
	return QStyledItemDelegate::createEditor(parent, option, index);
}

QString SeparatorDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
	if(value == CMD_SEPARATOR) {
		return QString(100, QChar(0x23AF)); // unicode horizontal line
	}
	return QStyledItemDelegate::displayText(value, locale);
}

void HotkeyEdit::keyPressEvent(QKeyEvent *e)
{
	if(!e) return;

	if(e->key() == Qt::Key_Escape || e->key() == Qt::Key_Enter) {
		return;
	}

	int keys = 0;
	bool has_modifier = false;
	auto addkey = [&keys, &has_modifier](int key) {
		has_modifier = true;
		keys += key;
	};
	if(e->modifiers() & Qt::AltModifier) {
		addkey(Qt::ALT);
	}
	if(e->modifiers() & Qt::ControlModifier) {
		addkey(Qt::CTRL);
	}
	if(e->modifiers() & Qt::ShiftModifier) {
		addkey(Qt::SHIFT);
	}

	auto key = e->key();

	if(key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt)
	{
		if(has_modifier) addkey(e->key());
	}

	m_sequence = QKeySequence(keys);

	QLineEdit::setText(m_sequence.toString());
}

HotkeyEdit::HotkeyEdit(QWidget *parent)
        : QLineEdit(parent)
{
}

HotkeyEdit::~HotkeyEdit()
{

}
