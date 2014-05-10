/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */
 
#include "picture.h"
#include <QResizeEvent>

static const char* TransparentBGFile = "://transparency.png";
static const char* BackgroundColor = "white"; // or #eef3fa

Picture::Picture(QWidget *parent) : QLabel(parent) {
	setFocusPolicy(Qt::ClickFocus);
	setMinimumSize(1,1);
	psize.setHeight(1);
	psize.setHeight(1);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	setStyleSheet(tr("background-color:%1;").arg(BackgroundColor));
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent *e) {Q_UNUSED(e)}
void Picture::dragMoveEvent(QDragMoveEvent *e) {Q_UNUSED(e)}
void Picture::dropEvent(QDropEvent *e) {Q_UNUSED(e)}

bool Picture::loadPicture(const QString &filename)
{
	if(!pixmap.load(filename))
		return false;

	if (pixmap.hasAlphaChannel()) {
		setStyleSheet(tr("background-image: url(%1);background-color: %2;").arg(TransparentBGFile).arg(BackgroundColor));
	} else {
		setStyleSheet(tr("background-image: none;background-color: %1;").arg(BackgroundColor));
	}

	resizeAndSetPixmap();
	return true;
}

QSize Picture::sizeHint() const
{
	return psize;
}

void Picture::resizeAndSetPixmap()
{

	psize.setWidth(qMin(size().width(),  pixmap.width()));
	psize.setHeight(qMin(size().height(),pixmap.height()));
	QLabel::setPixmap(pixmap.scaled(psize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}


void Picture::resizeEvent(QResizeEvent * e)
{
	Q_UNUSED(e);
	if(!pixmap.isNull()) {
		resizeAndSetPixmap();
	}
}
