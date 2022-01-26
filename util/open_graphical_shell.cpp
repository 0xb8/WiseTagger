/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "open_graphical_shell.h"
#include <QFile>

#ifdef Q_OS_WIN32
#include <QStringList>
#include <QApplication>
#include <QDir>
#include <QMultiMap>
#include <QFileInfo>
#include <QMessageBox>
#include <memory>
#include <Shlobj.h>
#else
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#endif


void util::open_files_in_gui_shell(const QStringList& input_files) {
#ifdef Q_OS_WIN32
	// store dir -> files in that dir
	QMultiMap<QString, QString> dirs_files;
	for (const auto& file : input_files) {
		auto fi = QFileInfo{file};
		if (!fi.exists())
			continue;

		auto dir = fi.absolutePath();
		dirs_files.insert(dir, file);
	}

	auto item_id_list_deleter = [](ITEMIDLIST* r) {
		::ILFree(r);
	};
	using item_id_list_t = std::unique_ptr<ITEMIDLIST, decltype(item_id_list_deleter)>;

	auto create_resource = [&item_id_list_deleter](const QString& path)
	{
		auto res = ::ILCreateFromPathW(QDir::toNativeSeparators(path).toStdWString().data());
		return item_id_list_t{res, item_id_list_deleter};
	};

	for (auto&& dir : dirs_files.uniqueKeys()) {
		const auto files = dirs_files.values(dir);

		std::vector<item_id_list_t> selection;
		for (const auto& file : files) {
			auto item = create_resource(file);
			Q_ASSERT(item);
			selection.push_back(std::move(item));
		}

		std::vector<const ITEMIDLIST*> selection_ptrs;
		for (const auto& sel : selection) {
			selection_ptrs.push_back(sel.get());
		}

		auto dir_item = create_resource(dir);
		Q_ASSERT(dir_item);
		auto res = ::SHOpenFolderAndSelectItems(dir_item.get(), selection_ptrs.size(), selection_ptrs.data(), 0);
		if (res != S_OK) {
			QMessageBox::critical(nullptr,
			                      qApp->translate("OpenInGuiShell","Error opening files in GUI shell"),
			                      qApp->translate("OpenInGuiShell","<p>Could not open files in GUI shell</p>"
			                                      "<p>System returned code %1").arg(res));
		}
	}

#else
	for (const auto& file : files)
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).path()));
#endif
}
