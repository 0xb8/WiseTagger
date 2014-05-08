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
