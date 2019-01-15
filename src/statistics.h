/* Copyright © 2016 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

/** @file statistics.h
 *  @brief Class @ref TaggerStatistics
 */

#include <QSettings>
#include <QObject>
#include <QElapsedTimer>
#include <QSize>

/// The TaggerStatistics singleton collects and displays various statistics.
class TaggerStatistics : public QObject {
	Q_OBJECT
public:

	/// Returns reference to an instance of TaggerStatistics
	static TaggerStatistics& instance();

public slots:
	/// Collects information about opened file.
	void fileOpened(const QString& filename, QSize dimensions);

	/// Collects information about added tags.
	void fileRenamed(const QString& new_name);

	/// Counts how many times reverse search was used.
	void reverseSearched();

	/// Collects the time taken to display cached pixmap.
	void pixmapLoadedFromCache(double time_ms);

	/// Collects the time taken to display directly loaded pixmap.
	void pixmapLoadedDirectly(double time_ms);

	/// Collects the time taken to display animated image.
	void movieLoadedDirectly(double time_ms);

	/// Displays dialog with all collected statistics.
	void showStatsDialog();

private:
	TaggerStatistics();
	~TaggerStatistics() override;

	QSettings m_settings;
	QElapsedTimer m_elapsed_timer;


	template<typename T>
	struct RollingAverage
	{
		size_t count = 0;
		T value = T{};


		void addSample(T sample)
		{
			if(count == 0) {
				value = old_value = sample;
			}
			++count;

			value = old_value + (sample - old_value) / count;
			old_value = value;
		}
	private:
		T old_value;
	};

	template<typename T>
	struct MinMax
	{
		T min = std::numeric_limits<T>::max();
		T max = std::numeric_limits<T>::min();
		bool hasMin() const
		{
			return min != std::numeric_limits<T>::max();
		}
		bool hasMax() const
		{
			return max != std::numeric_limits<T>::min();
		}

		void addSample(T sample)
		{
			if(sample > max)
				max = sample;
			if(sample < min)
				min = sample;
		}

	};

	RollingAverage<double> cached_pixmap_avg;
	RollingAverage<double> direct_pixmap_avg;
	RollingAverage<double> direct_movie_avg;
	MinMax<double>         cached_pixmap_minmax;
	MinMax<double>         direct_pixmap_minmax;
	MinMax<double>         direct_movie_minmax;

};

#endif
