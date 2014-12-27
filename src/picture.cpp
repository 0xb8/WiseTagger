/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#include "picture.h"
#include <QResizeEvent>

Picture::Picture(QWidget *_parent) : QLabel(_parent), resize_timer(this) {
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	setBackgroundStyle();
	connect(&resize_timer, SIGNAL(timeout()), this, SLOT(resizeTimeout()));
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent *e) {Q_UNUSED(e)}
void Picture::dragMoveEvent(QDragMoveEvent *e)   {Q_UNUSED(e)}
void Picture::dropEvent(QDropEvent *e)           {Q_UNUSED(e)}

bool Picture::loadPicture(const QString &filename)
{
	if(!pixmap.load(filename))
		return false;

	setBackgroundStyle(pixmap.hasAlphaChannel());
	resizeAndSetPixmap();
	return true;
}

QSize Picture::sizeHint() const
{
	return pixmap_size;
}

void Picture::resizeAndSetPixmap()
{
	pixmap_size.setWidth( qMin(size().width(),  pixmap.width()));
	pixmap_size.setHeight(qMin(size().height(), pixmap.height()));
	QLabel::setPixmap(pixmap.scaled(pixmap_size,
		Qt::KeepAspectRatio,
		Qt::SmoothTransformation));
}

/* Apply checkerboard background if pixmap has alpha channel */
void Picture::setBackgroundStyle(bool has_alpha)
{
	if(has_alpha)
		setStyleSheet(tr("background-image: url(%1);background-color: %2;").arg(TransparentBGFile).arg(BackgroundColor));

	else	setStyleSheet(tr("background-image: none;background-color: %1;").arg(BackgroundColor));
}

int Picture::width() const
{
	return pixmap.width();
}

int Picture::height() const
{
	return pixmap.height();
}

/* Restart timer on resize*/
void Picture::resizeEvent(QResizeEvent * e)
{
	Q_UNUSED(e);
	resize_timer.start(Picture::resize_timeout);
}

/* Perform actual pixmap resize on time out */
void Picture::resizeTimeout()
{
	resize_timer.stop();
	if(!pixmap.isNull()) {
		resizeAndSetPixmap();
	}
}
