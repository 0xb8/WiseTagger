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
#include <QMovie>
#include <memory>

class QResizeEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class Picture : public QLabel
{
	Q_OBJECT
public:
	explicit Picture(QWidget *parent = nullptr);

	/*!
	 * \brief Loads and displays image.
	 * \param filename Image to show.
	 * \retval true Loaded successfully.
	 * \retval false Failed to load image.
	 */
	bool loadPicture(const QString& filename);

	QSize sizeHint() const override;

	/// Returns dimensions of loaded image.
	QSize imageSize() const;

public slots:

	/// Clears image and displays welcome text.
	void clear();

protected:
	void dragEnterEvent(QDragEnterEvent*)	override;
	void dragMoveEvent(QDragMoveEvent*)	override;
	void dropEvent(QDropEvent*)	override;
	void resizeEvent(QResizeEvent*)	override;

private slots:
	void resizeTimeout();

private:
	enum class Type : std::int8_t {
		Image, Movie
	};

	void resizeAndSetPixmap();
	void setBackgroundStyle(bool has_alpha = false);

	QSize   m_current_size;
	QSize   m_initial_size;

	QPixmap m_pixmap;
	QTimer  m_resize_timer;
	std::unique_ptr<QMovie> m_movie;
	Type    m_which;

	static const int m_resize_timeout = 100; //ms
};

#endif // PICTURE_H
