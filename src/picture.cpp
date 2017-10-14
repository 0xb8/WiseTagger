/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "picture.h"
#include "statistics.h"
#include "util/misc.h"
#include <QSettings>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QApplication>
#include <QLoggingCategory>
#include <QThread>
#include <QElapsedTimer>

namespace logging_category {Q_LOGGING_CATEGORY(picture, "Picture")}
#define pinfo qCInfo(logging_category::picture)
#define pdbg qCDebug(logging_category::picture)
#define pwarn qCWarning(logging_category::picture)

static auto make_movie(QIODevice *d)
{
	return std::make_unique<QMovie>(d);
}

Picture::Picture(QWidget *parent) :
	QLabel{parent},
	m_widget_size{0,0},
	m_media_size{0,0},
	m_movie(nullptr),
	m_type{Type::WelcomeText},
	m_has_alpha{false}
{
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setObjectName(QStringLiteral("Picture"));

	// NOTE: support custom Qt themes
	QPalette pal(palette());
	pal.setColor(QPalette::Window, palette().color(QPalette::Base));
	auto textcolor{palette().color(QPalette::Text)};
	textcolor.setAlpha(140);
	pal.setColor(QPalette::WindowText, textcolor);

	setTextInteractionFlags(Qt::NoTextInteraction);
	setAutoFillBackground(true);
	setPalette(pal);

	connect(&m_resize_timer, &QTimer::timeout, this, [this]()
	{
		cache.clear();
		if(!m_pixmap.isNull()) {
			if(m_pixmap.size() != m_media_size) {
				pwarn << "pixmap not suitable for resize, reloading...";
				loadMedia(m_current_file);
				return;
			}
		}
		if(!m_pixmap.isNull() || (m_movie && m_movie->isValid()))
			resizeMedia();
	});
	m_resize_timer.setSingleShot(true);

	clear();
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent*) {}
void Picture::dragMoveEvent(QDragMoveEvent*)   {}
void Picture::dropEvent(QDropEvent*)           {}

bool Picture::loadMedia(const QString &filename)
{
	clearState();
	m_current_file = filename;

	if(tryLoadImageFromCache(filename))
		return true;

	QElapsedTimer timer;
	timer.start();

	QFile file(filename);
	bool open = file.open(QIODevice::ReadOnly);
	if(!open) {
		pwarn << "failed to open file for reading";
		return false;
	}
	m_file_buf.setData(file.readAll());
	file.close();

	QImageReader reader(&m_file_buf);

	if(reader.imageCount() > 1) {
		reader.jumpToImage(0);
		const auto first_frame = reader.read();

		m_media_size = first_frame.size();
		m_has_alpha  = first_frame.hasAlphaChannel();
		m_type       = Type::AnimatedImage;

		m_file_buf.reset();
		m_movie = make_movie(&m_file_buf);

		if(!m_movie->isValid()) {
			pwarn << "invalid movie";
			clear();
			return false;
		}

		m_movie->setCacheMode(QMovie::CacheNone);
		this->setMovie(m_movie.get());
		TaggerStatistics::instance().movieLoadedDirectly(timer.nsecsElapsed() / 1e6);
	} else {
		m_pixmap = QPixmap::fromImage(reader.read());
		if(m_pixmap.isNull()) {
			pwarn << "invalid pixmap";
			clear();
			return false;
		}

		m_media_size = m_pixmap.size();
		m_has_alpha  = m_pixmap.hasAlpha();
		m_type       = Type::Image;

		TaggerStatistics::instance().pixmapLoadedDirectly(timer.nsecsElapsed() / 1e6);
	}

	updateStyle();
	resizeMedia();
	return true;
}

QSize Picture::sizeHint() const
{
	return m_widget_size;
}

QSize Picture::mediaSize() const
{
	return m_media_size;
}

bool Picture::hasAlpha() const
{
	return m_has_alpha;
}

void Picture::resizeMedia()
{
	m_widget_size.setWidth( std::min(size().width(),  m_media_size.width()));
	m_widget_size.setHeight(std::min(size().height(), m_media_size.height()));
	float ratio = std::min(m_widget_size.width()  / static_cast<float>(m_media_size.width()),
	                       m_widget_size.height() / static_cast<float>(m_media_size.height()));
	m_widget_size = QSize(m_media_size.width() * ratio, m_media_size.height() * ratio);

	switch(m_type) {
		case Type::Image:

			if(m_pixmap.size() == m_widget_size)
			{
				QLabel::setPixmap(m_pixmap); // for pixmaps from cache
			} else {
				pdbg << "resizing pixmap from" << m_pixmap.size() << "to" << m_widget_size;
				QLabel::setPixmap(m_pixmap.scaled(m_widget_size,
				Qt::IgnoreAspectRatio,
				Qt::SmoothTransformation));
			}
			break;
		case Type::AnimatedImage:
			Q_ASSERT(m_movie != nullptr);
			m_movie->stop();
			m_movie->setScaledSize(m_widget_size);
			m_movie->jumpToFrame(0);
			m_movie->start();
			break;
		default: break;
	}
}

/** Applies checkerboard background if media has alpha channel. */
void Picture::updateStyle()
{
	if(hasAlpha()) {
		setStyleSheet(QStringLiteral("background-image: url(://transparency.png);"));
	} else {
		setStyleSheet(QStringLiteral("background-image: none;"));
	}
}

/** Sets data members to default values. */
void Picture::clearState()
{
	m_pixmap.loadFromData(nullptr);
	m_movie.reset(nullptr);
	m_file_buf.close();
	m_widget_size = m_media_size = QSize(0,0);
	m_type = Type::WelcomeText;
	m_has_alpha = false;
}

void Picture::clear()
{
	clearState();
	updateStyle();
	setText(util::read_resource_html("welcome.html"));
}

/* Restart timer on resize */
void Picture::resizeEvent(QResizeEvent*)
{
	m_resize_timer.start(resize_timeout);
}

bool Picture::tryLoadImageFromCache(const QString& filename)
{
	QSettings s;
	if(!s.value(QStringLiteral("performance/pixmap_precache_enabled"), false).toBool())
		return false;

	QElapsedTimer timer;
	timer.start();

	const int sleep_amount_ms = 10;
	const int sleep_timeout_ms = 5000;
	uint64_t unique_id = 0;

	while(true) {
		if(timer.elapsed() > sleep_timeout_ms) {
			pwarn << "pixmap query timed out after" << timer.elapsed() << "ms.";
			break;
		}

		auto query_result = cache.getPixmap(filename, unique_id);
		unique_id = query_result.unique_id;

		switch (query_result.result) {
		case ImageCache::Loading:
		case ImageCache::Evicted:
			QThread::msleep(sleep_amount_ms);
			continue;
		case ImageCache::Ready:
			m_pixmap     = query_result.pixmap;
			m_media_size = query_result.original_size;
			m_has_alpha  = m_pixmap.hasAlpha();
			m_type       = Type::Image;
			updateStyle();
			resizeMedia();
			TaggerStatistics::instance().pixmapLoadedFromCache(timer.nsecsElapsed() / 1e6);
			return true;
		default:
			break; // doesn't break the loop, kept so I remember that
		}
		break;
	}

	pinfo << "cache miss, loading directly...";
	return false;
}
