/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef PICTURE_H
#define PICTURE_H

#include <QTimer>
#include <QPixmap>
#include <QString>
#include <QLabel>

class QResizeEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class Picture : public QLabel
{
	Q_OBJECT
public:
	explicit Picture(QWidget *parent = nullptr);
	bool loadPicture(const QString& filename);

	QSize sizeHint() const override;
	int width() const;
	int height() const;

protected:
	void dragEnterEvent(QDragEnterEvent*)	override;
	void dragMoveEvent(QDragMoveEvent*)	override;
	void dropEvent(QDropEvent*)	override;
	void resizeEvent(QResizeEvent*)	override;

private slots:
	void resizeTimeout();

private:
	void resizeAndSetPixmap();
	void setBackgroundStyle(bool has_alpha = false);

	QPixmap m_pixmap;
	QSize   m_pixmap_size;
	QTimer  m_resize_timer;

	static const int m_resize_timeout = 100; //ms
	static constexpr const char* m_transparent_background_file = "://transparency.png";
	static constexpr const char* m_background_color = "white";
};

#endif // PICTURE_H
