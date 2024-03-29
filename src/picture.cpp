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
#include <QFile>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>

namespace logging_category {Q_LOGGING_CATEGORY(picture, "Picture")}
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
	m_movie{nullptr},
	m_type{Type::WelcomeText},
	m_rotation{0},
	m_has_alpha{false},
	m_upscale{false},
	m_status_left{this},
	m_status_right{this}
{
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setObjectName(QStringLiteral("Picture"));

	QGridLayout* layout = new QGridLayout(this);
	layout->addWidget(&m_status_left, 0, 0, Qt::AlignLeft | Qt::AlignTop);
	layout->addWidget(&m_status_right, 0, 0, Qt::AlignRight | Qt::AlignBottom);
	layout->setContentsMargins(3, 3, 3, 3);

	setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	setOpenExternalLinks(false);
	setAutoFillBackground(true);

	// NOTE: support custom Qt themes
	QPalette pal(palette());
	pal.setColor(QPalette::Window, qApp->palette().color(QPalette::Base));
	auto textcolor{palette().color(QPalette::Text)};
	textcolor.setAlpha(140);
	pal.setColor(QPalette::WindowText, textcolor);
	setPalette(pal);

	auto make_effect = [](QColor color){
		auto eff = new QGraphicsDropShadowEffect();
		eff->setEnabled(true);
		eff->setOffset(0, 0);
		eff->setBlurRadius(12);
		eff->setColor(color);
		return eff;
	};

	auto shadow_color = pal.color(QPalette::Base);
	QColor text_color;
	if (shadow_color.lightness() > 128) {
		shadow_color = Qt::white;
		text_color = QColor(0, 0, 0, 160);
	} else {
		shadow_color = Qt::black;
		text_color = QColor(255, 255, 255, 140);
	}

	QPalette pal2 = pal;
	pal2.setColor(QPalette::WindowText, text_color);

	m_status_left.setPalette(pal2);
	m_status_left.setGraphicsEffect(make_effect(shadow_color));
	m_status_left.setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	m_status_left.setOpenExternalLinks(false);
	m_status_right.setPalette(pal2);
	m_status_right.setGraphicsEffect(make_effect(shadow_color));
	m_status_right.setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	m_status_right.setOpenExternalLinks(false);

	connect(&m_status_left, &QLabel::linkActivated, this, &Picture::linkActivated);
	connect(&m_status_right, &QLabel::linkActivated, this, &Picture::linkActivated);

	connect(&m_resize_timer, &QTimer::timeout, this, [this]()
	{
		cache.clear();
		if(!m_pixmap.isNull()) {
			if(m_pixmap.size() != m_media_size || std::abs(m_pixmap.devicePixelRatioF() - devicePixelRatioF()) > 0.05f) {
				pwarn << "pixmap not suitable for resize, reloading...";
				loadMedia(m_current_file);
				return;
			}
		}
		if(!m_pixmap.isNull() || (m_movie && m_movie->isValid()))
			resizeMedia();
	});
	m_resize_timer.setSingleShot(true);

	m_status_font.setPointSize(8);
	m_status_left.setFont(m_status_font);
	m_status_right.setFont(m_status_font);
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

	if(m_rotation == 0 && tryLoadImageFromCache(filename))
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
	auto format = util::guess_image_format(filename);
	if(!format.isEmpty()) {
		reader.setFormat(format);
	}

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
		m_pixmap.setDevicePixelRatio(devicePixelRatioF());

		m_has_alpha  = m_pixmap.hasAlpha();
		m_type       = Type::Image;

		if (m_rotation) {
			QTransform t;
			t.rotate(90.0f * m_rotation);
			m_pixmap = m_pixmap.transformed(t, Qt::FastTransformation);
		}

		m_media_size = m_pixmap.size();

		TaggerStatistics::instance().pixmapLoadedDirectly(timer.nsecsElapsed() * 1e-6);
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

QSizeF Picture::mediaDisplaySize() const
{
	QSize size;

	switch (m_type) {
	case Type::AnimatedImage:
		size = m_widget_size;
		size *= devicePixelRatioF();
		break;
	case Type::Image:
		size = m_widget_size;
		break;
	default:
		break;
	}

	return size;
}

bool Picture::hasAlpha() const
{
	return m_has_alpha;
}

bool Picture::upscalingEnabled() const
{
	return m_upscale;
}

void Picture::setRotation(int steps)
{
	m_rotation = steps;
	if (m_type == Type::Image) {
		loadMedia(m_current_file);
	} else if (m_type == Type::AnimatedImage && (steps % 4) != 0) {
		pwarn << "Rotation not implemented for GIFs";
	}
}

int Picture::rotation() const
{
	return m_rotation;
}

void Picture::resizeMedia()
{
	// Pixmaps use separate scaling factor set when loading image, so we compensate here to be pixel-perfect.
	// GIFs don't have such scaling (and thus are not actually pixel-perfect), hence this check.
	const float device_pixel_ratio = (m_type == Type::Image) ? devicePixelRatioF() : 1.0f;
	const int viewport_width = size().width() * device_pixel_ratio;
	const int viewport_height = size().height() * device_pixel_ratio;

	if (m_upscale) {
		m_widget_size = m_media_size.scaled(viewport_width, viewport_height, Qt::KeepAspectRatio);
	} else {
		m_widget_size = m_media_size.scaled(std::min(viewport_width, m_media_size.width()),
		                                    std::min(viewport_height, m_media_size.height()),
		                                    Qt::KeepAspectRatio);
	}

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
	emit mediaResized();
}

/** Applies checkerboard background if media has alpha channel. */
void Picture::updateStyle()
{
	if(hasAlpha()) {
		setStyleSheet(QStringLiteral("QLabel#Picture { background-image: url(://transparency.png); }"));
	} else {
		setStyleSheet(QStringLiteral("QLabel#Picture { background-image: none; }"));
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

void Picture::setStatusText(const QString& left, const QString& right)
{
	m_status_left.setText(left);
	m_status_right.setText(right);
}

void Picture::setUpscalingEnabled(bool enabled)
{
	m_upscale = enabled;
	resizeMedia();
}

/* Restart timer on resize */
void Picture::resizeEvent(QResizeEvent*)
{
	m_resize_timer.start(resize_timeout);
}

bool Picture::tryLoadImageFromCache(const QString& filename)
{
	QSettings settings;
	if(!settings.value(QStringLiteral("performance/pixmap_precache_enabled"), true).toBool())
		return false;

	QElapsedTimer timer;
	timer.start();

	const int sleep_amount_ms = 20;
	const int sleep_timeout_ms = 5000;
	uint64_t unique_id = 0;

	while(true) {
		if(timer.elapsed() > sleep_timeout_ms) {
			pwarn << "pixmap query timed out after" << timer.elapsed() << "ms.";
			break;
		}

		auto query_result = cache.getImage(filename, this->size(), unique_id);
		unique_id = query_result.unique_id;

		switch (query_result.result) {
		case ImageCache::State::Loading:
			QThread::msleep(sleep_amount_ms);
			continue;
		case ImageCache::State::Ready:
			m_pixmap     = QPixmap::fromImage(query_result.image);
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

	pwarn << "cache miss:" << filename << "/" << unique_id <<", trying to load directly...";
	return false;
}
