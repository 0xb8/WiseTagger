/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef PICTURE_H
#define PICTURE_H

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

class Picture : public QLabel
{
	Q_OBJECT
public:
	/// Constructs Picture object and displays welcome text.
	explicit Picture(QWidget *parent = nullptr);

	/**
	 * \brief Loads and displays media.
	 * \param filename Media file to show.
	 * \retval true Loaded successfully.
	 * \retval false Failed to load media.
	 */
	bool loadMedia(const QString& filename);

	/// Returns whether loaded media has alpha channel.
	bool hasAlpha() const;

	/// Returns dimensions of loaded media.
	QSize mediaSize() const;

	QSize sizeHint() const override;

	ImageCache cache;

public slots:

	/// Clears media and displays welcome text.
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
