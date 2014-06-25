/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef PICTURE_H
#define PICTURE_H

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPixmap>
#include <QString>
#include <QLabel>

class Picture : public QLabel
{
	Q_OBJECT
public:
	explicit Picture(QWidget *parent = 0);
	bool loadPicture(const QString& filename);
	virtual QSize sizeHint() const;
	void resizeAndSetPixmap();
	int picture_width() const;
	int picture_height() const;

protected:
	void dragEnterEvent(QDragEnterEvent *e);
	void dragMoveEvent(QDragMoveEvent *e);
	void dropEvent(QDropEvent *e);
	void resizeEvent(QResizeEvent *event);

private:
	QPixmap pixmap;
	QSize psize;

};

#endif // PICTURE_H
