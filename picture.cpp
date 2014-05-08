#include "picture.h"
#include <QPixmap>
#include <QGraphicsPixmapItem>
#include <QResizeEvent>
#include <QtDebug>

Picture::Picture(QWidget *parent) :	QGraphicsView(parent) {
	setFrameStyle(QFrame::NoFrame);
	setRenderHint(QPainter::SmoothPixmapTransform, true);
	QGraphicsScene *scene = new QGraphicsScene(this);
	setScene(scene);
	setFocusPolicy(Qt::ClickFocus);
}

// Implement empty event handlers to allow filtering by MainWindow
void Picture::dragEnterEvent(QDragEnterEvent *e) {Q_UNUSED(e)}
void Picture::dragMoveEvent(QDragMoveEvent *e) {Q_UNUSED(e)}
void Picture::dropEvent(QDropEvent *e) {Q_UNUSED(e)}

bool Picture::loadPicture(const QString &filename)
{
	QPixmap pm;
	if(!pm.load(filename))
		return false;

	pixmap_w = pm.width();
	pixmap_h = pm.height();

	scene()->clear();

	QGraphicsPixmapItem *item = scene()->addPixmap(pm);
	item->setTransformationMode(Qt::SmoothTransformation);
	scene()->setSceneRect(0, 0, pixmap_w, pixmap_h);

	if(pixmap_w > geometry().width() || pixmap_h > geometry().height()) {
		fitInView(0, 0, pixmap_w, pixmap_h, Qt::KeepAspectRatio);
	} else {
		fitInView(0, 0, geometry().width(), geometry().height(), Qt::KeepAspectRatio);
	}
	return true;
}


void Picture::resizeEvent(QResizeEvent *event)
{
	if(pixmap_w > geometry().width() || pixmap_h > geometry().height())
		fitInView(0, 0, pixmap_w, pixmap_h, Qt::KeepAspectRatio);
	else
		fitInView(0, 0, event->size().width(), event->size().height(), Qt::KeepAspectRatio);
		scene()->setSceneRect(0, 0, pixmap_w, pixmap_h);
}
