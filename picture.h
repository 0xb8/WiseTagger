#ifndef PICTURE_H
#define PICTURE_H

#include <QGraphicsPixmapItem>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QGraphicsView>
#include <QDropEvent>
#include <QPixmap>
#include <QString>

class Picture : public QGraphicsView
{
	Q_OBJECT
public:
	explicit Picture(QWidget *parent = 0);
	bool loadPicture(const QString& filename);

protected:
	void dragEnterEvent(QDragEnterEvent *e);
	void dragMoveEvent(QDragMoveEvent *e);
	void dropEvent(QDropEvent *e);

private:
	int pixmap_w;
	int pixmap_h;
	void resizeEvent(QResizeEvent *event);

};

#endif // PICTURE_H
