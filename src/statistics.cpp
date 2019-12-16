/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "statistics.h"
#include "util/size.h"
#include "util/misc.h"
#include <cmath>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>

namespace logging_category {Q_LOGGING_CATEGORY(stats, "Statistics")}
#define pdbg qCDebug(logging_category::stats)
#define pwarn qCWarning(logging_category::stats)

#define SETT_STATISTICS         QStringLiteral("stats/enabled")
#define NUM_TIMES_LAUNCHED      QStringLiteral("stats/num_times_launched")
#define NUM_REVERSE_SEARCHED    QStringLiteral("stats/num_reverse_searched")
#define NUM_FILES_OPENED        QStringLiteral("stats/num_files/opened")
#define NUM_FILES_RENAMED       QStringLiteral("stats/num_files/renamed")
#define NUM_TAGS_TOTAL          QStringLiteral("stats/num_tags/total")
#define NUM_TAGS_MAX            QStringLiteral("stats/num_tags/maximum")
#define NUM_TAGS_AVG            QStringLiteral("stats/num_tags/average")
#define NUM_TAGS_TOTAL          QStringLiteral("stats/num_tags/total")

#define IMG_DIMENSIONS_SUM_W    QStringLiteral("stats/img_dimensions/sum/width")
#define IMG_DIMENSIONS_SUM_H    QStringLiteral("stats/img_dimensions/sum/height")
#define IMG_DIMENSIONS_MAX      QStringLiteral("stats/img_dimensions/max")
#define IMG_DIMENSIONS_AVG      QStringLiteral("stats/img_dimensions/avg")

#define NUM_FILES_EXT           QStringLiteral("stats/num_files/extensions/%1")
#define NUM_FILES_EXT_DIR       QStringLiteral("stats/num_files/extensions")
#define TIME_SPENT              QStringLiteral("stats/time_spent_seconds")
#define DATE_FIRST_LAUNCHED     QStringLiteral("stats/date_first_launched")

TaggerStatistics &TaggerStatistics::instance()
{
	static TaggerStatistics instance;
	return instance;
}


TaggerStatistics::TaggerStatistics() : QObject(nullptr)
{
	m_elapsed_timer.start();
	if(m_settings.value(DATE_FIRST_LAUNCHED).isNull()) {
		m_settings.setValue(DATE_FIRST_LAUNCHED, QDateTime::currentDateTimeUtc());
	}
}

TaggerStatistics::~TaggerStatistics()
{
	if(!m_settings.value(SETT_STATISTICS, true).toBool()) {
		return;
	}
	auto times_launched = m_settings.value(NUM_TIMES_LAUNCHED, 0).toInt();
	auto time_spent = m_settings.value(TIME_SPENT, 0).toLongLong();
	time_spent += m_elapsed_timer.elapsed() / 1000;
	m_settings.setValue(TIME_SPENT, time_spent);
	m_settings.setValue(NUM_TIMES_LAUNCHED, ++times_launched);
}


void TaggerStatistics::fileOpened(const QString &filename, QSize dimensions)
{
	if(!m_settings.value(SETT_STATISTICS, true).toBool()) {
		return;
	}
	auto files_opened = m_settings.value(NUM_FILES_OPENED, 0).toInt();
	m_settings.setValue(NUM_FILES_OPENED, ++files_opened);

	auto dimensions_sum_w = m_settings.value(IMG_DIMENSIONS_SUM_W, 0ll).toLongLong();
	auto dimensions_sum_h = m_settings.value(IMG_DIMENSIONS_SUM_H, 0ll).toLongLong();
	dimensions_sum_w += dimensions.width();
	dimensions_sum_h += dimensions.height();
	m_settings.setValue(IMG_DIMENSIONS_SUM_W, dimensions_sum_w);
	m_settings.setValue(IMG_DIMENSIONS_SUM_H, dimensions_sum_h);

	auto new_dimensions_avg = QSize(dimensions_sum_w/files_opened, dimensions_sum_h/files_opened);
	m_settings.setValue(IMG_DIMENSIONS_AVG, new_dimensions_avg);

	auto dimensions_max = m_settings.value(IMG_DIMENSIONS_MAX, QSize(0,0)).toSize();
	if(std::hypot(dimensions.width(), dimensions.height())
		> std::hypot(dimensions_max.width(), dimensions_max.height()))
	{
		m_settings.setValue(IMG_DIMENSIONS_MAX, dimensions);
	}

	QFileInfo fi(filename);
	const auto sett_ext = QString(NUM_FILES_EXT).arg(fi.suffix());

	auto ext_count = m_settings.value(sett_ext, 0).toInt();
	m_settings.setValue(sett_ext, ++ext_count);
}

void TaggerStatistics::fileRenamed(const QString &new_name)
{
	if(!m_settings.value(SETT_STATISTICS, true).toBool()) {
		return;
	}
	auto files_renamed = m_settings.value(NUM_FILES_RENAMED, 0ll).toLongLong();
	auto tags_max      = m_settings.value(NUM_TAGS_MAX, 0ll).toLongLong();
	auto tag_count     = std::count(new_name.cbegin(), new_name.cend(),' ');
	auto tags_total    = m_settings.value(NUM_TAGS_TOTAL, 0ll).toLongLong();

	tags_total += tag_count + 1; // spaces are between tags, so we add one
	files_renamed += 1;

	m_settings.setValue(NUM_TAGS_TOTAL, tags_total);
	if(tag_count > tags_max) {
		m_settings.setValue(NUM_TAGS_MAX, static_cast<decltype(tags_max)>(tag_count));
	}

	Q_ASSERT(files_renamed > 0);
	m_settings.setValue(NUM_FILES_RENAMED, files_renamed);
	m_settings.setValue(NUM_TAGS_AVG, tags_total / files_renamed);
}

void TaggerStatistics::reverseSearched()
{
	if(!m_settings.value(SETT_STATISTICS, true).toBool()) {
		return;
	}
	auto num = m_settings.value(NUM_REVERSE_SEARCHED, 0).toLongLong();
	m_settings.setValue(NUM_REVERSE_SEARCHED, num + 1);
}

void TaggerStatistics::pixmapLoadedFromCache(double time_ms)
{
	cached_pixmap_avg.addSample(time_ms);
	cached_pixmap_minmax.addSample(time_ms);
}

void TaggerStatistics::pixmapLoadedDirectly(double time_ms)
{
	direct_pixmap_avg.addSample(time_ms);
	direct_pixmap_minmax.addSample(time_ms);
}

void TaggerStatistics::movieLoadedDirectly(double time_ms)
{
	direct_movie_avg.addSample(time_ms);
	direct_movie_minmax.addSample(time_ms);
}

void TaggerStatistics::showStatsDialog()
{
	auto time_spent = m_settings.value(TIME_SPENT, 0).toLongLong();
	if(m_settings.value(SETT_STATISTICS, true).toBool()) {
		time_spent += m_elapsed_timer.elapsed() / 1000;
		m_settings.setValue(TIME_SPENT, time_spent);
		m_elapsed_timer.start();
	}
	const auto first_launch     = m_settings.value(DATE_FIRST_LAUNCHED).toDateTime().toLocalTime();
	const auto first_launch_dt  = first_launch.daysTo(QDateTime::currentDateTime());
	const auto first_launch_s   = first_launch.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
	const auto first_launch_dts = qApp->translate("Duration", "%n days", "", first_launch_dt);
	const auto files_opened     = m_settings.value(NUM_FILES_OPENED, 0).toInt();
	const auto dimensions_max   = m_settings.value(IMG_DIMENSIONS_MAX, QSize(0,0)).toSize();
	const auto dimensions_avg   = m_settings.value(IMG_DIMENSIONS_AVG, QSize(0,0)).toSize();
	const auto times_launched_s = m_settings.value(NUM_TIMES_LAUNCHED, 0).toString();
	const auto files_renamed_s  = m_settings.value(NUM_FILES_RENAMED, 0ll).toString();
	const auto tags_total_s     = m_settings.value(NUM_TAGS_TOTAL, 0ll).toString();
	const auto tags_max_s       = m_settings.value(NUM_TAGS_MAX, 0ll).toString();
	const auto tags_avg_s       = m_settings.value(NUM_TAGS_AVG, 0ll).toString();
	const auto rev_searched_s   = m_settings.value(NUM_REVERSE_SEARCHED, 0).toString();
	const auto files_opened_s   = QString::number(files_opened);

	const auto dim_max_str    = QStringLiteral("%1 x %2")
		.arg(dimensions_max.width()).arg(dimensions_max.height());
	const auto dim_avg_str    = QStringLiteral("%1 x %2")
		.arg(dimensions_avg.width()).arg(dimensions_avg.height());

	// populate extensions list
	m_settings.beginGroup(NUM_FILES_EXT_DIR);
	auto ext_list = m_settings.childKeys();
	std::sort(ext_list.begin(), ext_list.end(), [this](const auto& a, const auto& b)
	{
		return m_settings.value(a).toInt() > m_settings.value(b).toInt();
	});

	const auto exts_html = util::read_resource_html("statistics_extensions.html");
	QString exts_str;
	for(auto& key : qAsConst(ext_list)) {
		const auto num_exts = m_settings.value(key, 0).toInt();
		const auto percent_ext = QString::number(util::size::percent(num_exts, files_opened));
		exts_str += exts_html.arg(key.toUpper(), QString::number(num_exts), percent_ext);
	}
	m_settings.endGroup();

	auto desc = util::read_resource_html("statistics.html")
		.arg(times_launched_s,	// 1
		     util::duration(time_spent), // 2
		     files_opened_s,	// 3
		     files_renamed_s)	// 4
		.arg(rev_searched_s,	// 5
		     dim_max_str,	// 6
		     dim_avg_str,	// 7
		     exts_str,		// 8
		     tags_total_s,	// 9
		     tags_max_s,	// 10
		     tags_avg_s,	// 11
		     first_launch_s,	// 12
		     first_launch_dts	// 13
		);

	if(!m_settings.value(SETT_STATISTICS, true).toBool()) {
		desc.append(tr("<br/><p><b>Note:</b> statistics collection is currently disabled. "
		               "The stats displayed here are from previous launches.</p>"));
	}

	if(direct_pixmap_avg.count || cached_pixmap_avg.count || direct_movie_avg.count) {
		QLocale loc;
		auto min_direct = direct_pixmap_minmax.hasMin() ? loc.toString(direct_pixmap_minmax.min, 'f', 2) : QStringLiteral("0");
		auto max_direct = direct_pixmap_minmax.hasMax() ? loc.toString(direct_pixmap_minmax.max, 'f', 2) : QStringLiteral("0");
		auto min_cached = cached_pixmap_minmax.hasMin() ? loc.toString(cached_pixmap_minmax.min, 'f', 2) : QStringLiteral("0");
		auto max_cached = cached_pixmap_minmax.hasMax() ? loc.toString(cached_pixmap_minmax.max, 'f', 2) : QStringLiteral("0");
		auto min_movie  = direct_movie_minmax.hasMin()  ? loc.toString(direct_movie_minmax.min,  'f', 2) : QStringLiteral("0");
		auto max_movie  = direct_movie_minmax.hasMax()  ? loc.toString(direct_movie_minmax.max,  'f', 2) : QStringLiteral("0");
		auto avg_direct = direct_pixmap_avg.count       ? loc.toString(direct_pixmap_avg.value,  'f', 2) : QStringLiteral("0");
		auto avg_cached = cached_pixmap_avg.count       ? loc.toString(cached_pixmap_avg.value,  'f', 2) : QStringLiteral("0");
		auto avg_movie  = direct_movie_avg.count        ? loc.toString(direct_movie_avg.value,   'f', 2) : QStringLiteral("0");

		QString perf_text;
		if(direct_pixmap_avg.count) {
			perf_text.append(util::read_resource_html("performance_direct_pixmap.html").arg(
				QString::number(direct_pixmap_avg.count),
				avg_direct,
				min_direct,
				max_direct));
		}
		if(cached_pixmap_avg.count) {
			perf_text.append(util::read_resource_html("performance_cached_pixmap.html").arg(
				QString::number(cached_pixmap_avg.count),
				avg_cached,
				min_cached,
				max_cached));
		}
		if(direct_movie_avg.count)
		{
			perf_text.append(util::read_resource_html("performance_direct_movie.html").arg(
				QString::number(direct_movie_avg.count),
				avg_movie,
				min_movie,
				max_movie));
		}
		desc.append(util::read_resource_html("performance.html").arg(perf_text));
	}

	QMessageBox mb(QMessageBox::Information, tr("Statistics"), desc, QMessageBox::Ok, nullptr);
	mb.setTextInteractionFlags(Qt::TextBrowserInteraction);
	mb.exec();
}
