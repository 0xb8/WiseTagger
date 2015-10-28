/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "picture.h"
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

Picture::Picture(QWidget *parent) : QLabel(parent) {
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setBackgroundStyle();
	connect(&m_resize_timer, &QTimer::timeout, this, &Picture::resizeTimeout);
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent*) {}
void Picture::dragMoveEvent(QDragMoveEvent*)   {}
void Picture::dropEvent(QDropEvent*)           {}

bool Picture::loadPicture(const QString &filename)
{
	if(!m_pixmap.load(filename))
		return false;

	setBackgroundStyle(m_pixmap.hasAlphaChannel());
	resizeAndSetPixmap();
	return true;
}

QSize Picture::sizeHint() const
{
	return m_pixmap_size;
}

void Picture::resizeAndSetPixmap()
{
	m_pixmap_size.setWidth( std::min(size().width(),  m_pixmap.width()));
	m_pixmap_size.setHeight(std::min(size().height(), m_pixmap.height()));
	QLabel::setPixmap(m_pixmap.scaled(m_pixmap_size,
		Qt::KeepAspectRatio,
		Qt::SmoothTransformation));
}

/* Apply checkerboard background if pixmap has alpha channel */
void Picture::setBackgroundStyle(bool has_alpha)
{
	if(has_alpha)
		setStyleSheet(tr("background-image: url(%1);background-color: %2;").arg(m_transparent_background_file).arg(m_background_color));

	else	setStyleSheet(tr("background-image: none;background-color: %1;").arg(m_background_color));
}

int Picture::width() const
{
	return m_pixmap.width();
}

int Picture::height() const
{
	return m_pixmap.height();
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
	if(!m_pixmap.isNull()) {
		resizeAndSetPixmap();
	}
}
