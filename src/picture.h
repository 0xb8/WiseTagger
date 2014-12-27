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
#include <QTimer>
#include <QPixmap>
#include <QString>
#include <QLabel>

class Picture : public QLabel
{
	Q_OBJECT
public:
	explicit Picture(QWidget *_parent = nullptr);
	bool loadPicture(const QString& filename);

	virtual QSize sizeHint() const;
	int width() const;
	int height() const;

protected:
	void dragEnterEvent(QDragEnterEvent *e);
	void dragMoveEvent(QDragMoveEvent *e);
	void dropEvent(QDropEvent *e);
	void resizeEvent(QResizeEvent *event);

private slots:
	void resizeTimeout();

private:
	QPixmap pixmap;
	QSize pixmap_size;
	QTimer resize_timer;

	void resizeAndSetPixmap();
	void setBackgroundStyle(bool has_alpha = false);

	static const int resize_timeout = 100; //ms
	static constexpr char const* TransparentBGFile = "://transparency.png";
	static constexpr char const* BackgroundColor = "white";
};

#endif // PICTURE_H
