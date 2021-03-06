/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef PICTURE_H
#define PICTURE_H

/** @file picture.h
 *  @brief Class @ref Picture
 */

#include <memory>
#include <QBuffer>
#include <QLabel>
#include <QMovie>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include "util/imagecache.h"

class QResizeEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;


/// Widget for displaying images and GIFs
class Picture : public QLabel
{
	Q_OBJECT
public:
	/// Construct Picture object and display welcome text.
	explicit Picture(QWidget *parent = nullptr);

	/*!
	 * \brief Load and display media.
	 * \param filename Media file to show.
	 * \retval true Loaded successfully.
	 * \retval false Failed to load media.
	 */
	bool loadMedia(const QString& filename);

	/// Does the current media has alpha channel.
	bool hasAlpha() const;

	/// Dimensions of loaded media.
	QSize mediaSize() const;

	/// Size hint of the widget wrt. media size
	QSize sizeHint() const override;

	/// \ref ImageCache object
	ImageCache cache;

public slots:

	/// Clear media and display welcome text.
	void clear();

protected:
	void dragEnterEvent(QDragEnterEvent*) override;
	void dragMoveEvent(QDragMoveEvent*)   override;
	void dropEvent(QDropEvent*)           override;
	void resizeEvent(QResizeEvent*)       override;

private:
	enum class Type {
		WelcomeText,
		Image,
		AnimatedImage
	};

	using MoviePtr = std::unique_ptr<QMovie>;

	static constexpr int resize_timeout = 100; //ms

	bool tryLoadImageFromCache(const QString& filename);
	void resizeMedia();
	void updateStyle();
	void clearState();

	QString   m_current_file;
	QSize     m_widget_size;
	QSize     m_media_size;
	QTimer    m_resize_timer;
	QBuffer   m_file_buf;
	QPixmap   m_pixmap;
	MoviePtr  m_movie;
	Type      m_type;
	bool      m_has_alpha;

};

#endif // PICTURE_H
