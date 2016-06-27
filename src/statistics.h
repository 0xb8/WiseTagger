/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <QSettings>
#include <QObject>
#include <QElapsedTimer>

class TaggerStatistics : public QObject {
	Q_OBJECT
public:
	explicit TaggerStatistics(QObject *_parent = nullptr);
	~TaggerStatistics() override;

public slots:
	/// Collects information about opened file.
	void fileOpened(const QString& filename, QSize dimensions);

	/// Collects information about added tags.
	void fileRenamed(const QString& new_name);

	/// Counts how many times reverse search was used.
	void reverseSearched();

	/// Displays dialog with all collected statistics.
	void showStatsDialog();

private:
	QSettings m_settings;
	QElapsedTimer m_elapsed_timer;
};

#endif
