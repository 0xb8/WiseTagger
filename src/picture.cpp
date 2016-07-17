/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "picture.h"
#include "util/misc.h"
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QApplication>
#include <QLoggingCategory>

namespace logging_category {
	Q_LOGGING_CATEGORY(picture, "Picture")
}
#define pdbg qCDebug(logging_category::picture)
#define pwarn qCWarning(logging_category::picture)

Picture::Picture(QWidget *parent) : QLabel(parent), m_movie(nullptr) {
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setBackgroundStyle();
	connect(&m_resize_timer, &QTimer::timeout, this, &Picture::resizeTimeout);
	clear();
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent*) {}
void Picture::dragMoveEvent(QDragMoveEvent*)   {}
void Picture::dropEvent(QDropEvent*)           {}

bool Picture::loadPicture(const QString &filename)
{
	if(filename.endsWith(QStringLiteral(".gif"))) {
		m_movie_buf.close();
		QFile file(filename);
		bool open = file.open(QIODevice::ReadOnly);
		if(!open) {
			clear();
			return false;
		}

		m_movie_buf.setData(file.readAll());
		file.close();
		m_movie = std::make_unique<QMovie>(&m_movie_buf);
		if(!m_movie->isValid()) {
			clear();
			return false;
		}
		m_movie->setCacheMode(QMovie::CacheAll);
		QLabel::setMovie(m_movie.get());
		movie()->start();
		const auto cpm = movie()->currentPixmap();
		m_initial_size = cpm.size();
		m_type = cpm.hasAlpha() ? Type::MovieWithAlpha : Type::Movie;
	} else {
		if(!m_pixmap.load(filename)) {
			clear();
			return false;
		}
		m_initial_size = m_pixmap.size();
		m_type = m_pixmap.hasAlpha() ? Type::ImageWithAlpha : Type::Image;
	}
	setBackgroundStyle();
	resizeAndSetPixmap();
	return true;
}

QSize Picture::sizeHint() const
{
	return m_current_size;
}

QSize Picture::imageSize() const
{
	return m_initial_size;
}

void Picture::resizeAndSetPixmap()
{
	m_current_size.setWidth( std::min(size().width(),  m_initial_size.width()));
	m_current_size.setHeight(std::min(size().height(), m_initial_size.height()));
	float ratio = std::min(m_current_size.width()  / static_cast<float>(m_initial_size.width()),
			       m_current_size.height() / static_cast<float>(m_initial_size.height()));
	m_current_size = QSize(m_initial_size.width() * ratio, m_initial_size.height() * ratio);

	switch(m_type) {
		case Type::Image:
		case Type::ImageWithAlpha:
			QLabel::setPixmap(m_pixmap.scaled(m_current_size,
				Qt::IgnoreAspectRatio,
				Qt::SmoothTransformation));
			break;
		case Type::Movie:
		case Type::MovieWithAlpha:
			m_movie->setCacheMode(QMovie::CacheNone);
			m_movie->setScaledSize(m_current_size);
			m_movie->setCacheMode(QMovie::CacheAll);
			break;
		default:
			pwarn << "unknown type";
	}
}

/* Apply checkerboard background if pixmap has alpha channel */
void Picture::setBackgroundStyle()
{
	switch(m_type) {
		case Type::ImageWithAlpha:
		case Type::MovieWithAlpha:
			setStyleSheet(QStringLiteral("background-image: url(://transparency.png);"));
			break;
		default:
			setStyleSheet(QStringLiteral("background-image: none;"));
	}
}

void Picture::clear()
{
	m_pixmap.loadFromData(nullptr);
	m_movie.reset(nullptr);
	m_movie_buf.close();
	m_current_size = m_initial_size = QSize(0,0);
	m_type = Type::None;
	setText(util::read_resource_html("welcome.html"));
}

/* Restart timer on resize */
void Picture::resizeEvent(QResizeEvent*)
{
	m_resize_timer.start(Picture::m_resize_timeout);
}

/* Perform actual pixmap resize on time out */
void Picture::resizeTimeout()
{
	m_resize_timer.stop();
	if(!m_pixmap.isNull() || (m_movie && m_movie->isValid())) {
		resizeAndSetPixmap();
	}
}
