/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef WINDOW_H
#define WINDOW_H

/*! \file window.h
 *  \brief Class @ref Window
 */

#include "reverse_search.h"
#include "tagger.h"
#include "settings_dialog.h"
#include "filter_dialog.h"
#include "util/project_info.h"
#include <vector>
#include <QMainWindow>
#include <QAction>
#include <QString>
#include <QMenu>
#include <QStatusBar>
#include <QSystemTrayIcon>

#ifdef Q_OS_WIN32
#include <QtWinExtras>
#endif

/*!
 * \brief Application's main window
 */
class Window : public QMainWindow {
	Q_OBJECT
public:
	explicit Window(QWidget *_parent = nullptr);
	~Window() override = default;

public slots:
	/// Display current upload progress in window title
	void showUploadProgress(qint64 bytesSent, qint64 bytesTotal);

	/// Display current tag fetching progress
	void showTagFetchProgress(QString url);

	/// Display file hashing progress.
	void showFileHashingProgress(QString file, int value);

	/// Hide current upload progress
	void hideUploadProgress();

	/// Update the window's title
	void updateWindowTitle();

	/// Update the window's title to show progress in percent.
	void updateWindowTitleProgress(int progress);

	/// Update the status bar
	void updateStatusBarText();

	/// Update current file's imageboard post URL
	void updateImageboardPostURL(QString url);

	/*!
	 * \brief Submit new notification
	 *
	 * Notifications are queued and are shown in a submenu of main menu bar.
	 * \param title Notification title
	 * \param description Short description (optional).
	 * \param body Main notification text (optional).
	 */
	void addNotification(QString title, QString description=QString{}, QString body=QString{});

	/// Remove notification from queue
	void removeNotification(QString title);

	/// Update settings
	void updateSettings();

	/// Close the window and restart the process.
	void scheduleRestart();

protected:
	bool eventFilter(QObject*, QEvent *) override;
	void closeEvent(QCloseEvent*) override;
	void showEvent(QShowEvent*) override;
	void hideEvent(QHideEvent*) override;

private slots:
	void fileOpenDialog();
	void directoryOpenDialog();
	void updateProxySettings();
	void about();
	void help();

private:
	void createActions();
	void createMenus();
	void createCommands();
	void updateMenus();
	void parseCommandLineArguments();

	void initSettings();

	void saveSettings();
	void updateStyle();
	void setViewMode(ViewMode mode);
	void setSlideShow(bool enabled);

	void showNotificationsMenu();
	void hideNotificationsMenu();

	void restartProcess(QString current_text);
	void readRestartData();

	static constexpr const char* MainWindowTitle = "%1%2 [%3x%4] (%5)  –  " TARGET_PRODUCT " v%6";
	static constexpr const char* MainWindowTitleFrameRate = "%1%2 [%3x%4, %7 FPS] (%5)  –  " TARGET_PRODUCT " v%6";
	static constexpr const char* MainWindowTitleEmpty = TARGET_PRODUCT " v%1";
	static constexpr const char* MainWindowTitleProgress = "%7%  –  %1%2 [%3x%4] (%5)  –  " TARGET_PRODUCT " v%6";

	Tagger m_tagger;
	ReverseSearch m_reverse_search;

	QString m_last_directory;
	QString m_post_url;

	QAction a_open_file;
	QAction a_open_dir;
	QAction a_delete_file;
	QAction a_open_post;
	QAction a_iqdb_search;
	QAction a_exit;
	QAction a_hide;
	QAction a_set_queue_filter;
	QAction a_next_file;
	QAction a_prev_file;
	QAction a_save_file;
	QAction a_save_next;
	QAction a_save_prev;
	QAction a_go_to_number;
	QAction a_open_session;
	QAction a_save_session;
	QAction a_fix_tags;
	QAction a_fetch_tags;
	QAction a_open_loc;
	QAction a_reload_tags;
	QAction a_open_tags;
	QAction a_edit_tags;
	QAction a_edit_temp_tags;
	QAction a_ib_replace;
	QAction a_ib_restore;
	QAction a_tag_forcefirst;
	QAction a_show_settings;
	QAction a_view_normal;
	QAction a_view_minimal;
	QAction a_view_statusbar;
	QAction a_view_fullscreen;
	QAction a_exit_fullscreen;
	QAction a_view_slideshow;
	QAction a_exit_slideshow;
	QAction a_view_menu;
	QAction a_view_input;
	QAction a_view_sort_name;
	QAction a_view_sort_type;
	QAction a_view_sort_date;
	QAction a_view_sort_size;
	QAction a_play_pause;
	QAction a_play_mute;
	QAction a_about;
	QAction a_about_qt;
	QAction a_help;
	QAction a_stats;
	QActionGroup ag_sort_criteria;

	QAction* a_menu_commands_action = nullptr;
	QAction* a_menu_play_action = nullptr;

	QMenu menu_file;
	QMenu menu_navigation;
	QMenu menu_view;
	QMenu menu_play;
	QMenu menu_sort;
	QMenu menu_options;
	QMenu menu_commands;
	QMenu menu_help;
	QMenu menu_notifications;
	QMenu menu_tray;
	QMenu menu_context_tagger;

	QStatusBar m_statusbar;
	QLabel     m_statusbar_label;
	QTimer     m_notification_display_timer;
	QSystemTrayIcon m_tray_icon;
	ViewMode   m_view_mode;
	int        m_notification_count = 0;
	bool       m_show_current_directory = false;
	bool       m_view_maximized = false;

	FilterDialog m_filter_dialog;

#ifdef Q_OS_WIN32
	QWinTaskbarButton       m_win_taskbar_button;
	QNetworkAccessManager   m_vernam;
	void checkNewVersion();
	void processNewVersion(QNetworkReply*);
#endif
};
#endif // WINDOW_H
