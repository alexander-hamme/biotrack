#include "paintcanvas.h"

#include <iostream>
#include <QtGui>
PaintCanvas::PaintCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents);
    modified = false;
    scribbling = false;
    targetpen=true;
    backgroundpen=false;
    backgroundmaskpen=false;
    markHead=false;
    markTail=false;
    myPenWidth = 5;
    myPenWidth++;
    myPenColor = Qt::blue;
    myPenColor =Qt::green;
    myBrushColor =Qt::gray;
    maskpathnewpath=true;
    clickedToEndPath=false;

}

bool PaintCanvas::openImage(const QString &fileName)
{
    QImage loadedImage;
    if (!loadedImage.load(fileName))
        return false;

    QSize newSize = loadedImage.size().expandedTo(size());
    resizeImage(&loadedImage, newSize);
    image = loadedImage;
    modified = false;
    update();
    return true;
}



void PaintCanvas::setPenColor(const QColor &newColor)
{
    myPenColor = newColor;
}

void PaintCanvas::setPenWidth(int newWidth)
{
    myPenWidth = newWidth;
}
void PaintCanvas::setBrushColor(const QColor &newColor)
{
    myBrushColor = newColor;
}

void PaintCanvas::clearImage()
{
    image.fill(qRgba(255, 255, 255,0));
    modified = true;
    maskpath= QPainterPath();
    clickedToEndPath=false;
    maskpathnewpath=true;

    update();
    std::cout<<"Cleared  "<<myPenWidth<<std::endl;
}

void PaintCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {

        clickedToEndPath=false;

        //For mask drawing
        if (backgroundmaskpen){
            if(maskpathnewpath){
                maskpath.moveTo(event->pos());
                maskpathnewpath=false;
                lastPoint = event->pos();

            }

            maskpath.lineTo(event->pos());
            drawLineTo(event->pos());

            update();

        }
        //For head and Tail marking
        if(markHead){
            scribbling = true;
        lastPoint = event->pos();
        //THEN SWITCH TO OTHER PEN

        }

        if(markTail){
            scribbling = true;
        lastPoint = event->pos();
        //THEN SWITCH TO OTHER PEN

        }
        //Default other drawing
        if(targetpen||backgroundpen)
            scribbling = true;
        lastPoint = event->pos();


    }
    if (event->button()==Qt::RightButton&&backgroundmaskpen){
        //Close off current path
        closepath();
    }
}

void PaintCanvas::closepath(void)
{//Close off current path
    maskpath.closeSubpath();
    maskpathnewpath=true;
    clickedToEndPath=true;
    drawLineTo(lastPoint);
   update();
}

void PaintCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && scribbling&&(targetpen||backgroundpen))
        drawLineTo(event->pos());
}

void PaintCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && scribbling&&(targetpen||backgroundpen)) {
        drawLineTo(event->pos());
        scribbling = false;
    }

    if (event->button() == Qt::LeftButton && scribbling&&(markHead)) {//Draw head dot
        drawMarkerDot(event->pos());
        scribbling = false;
    }

    if (event->button() == Qt::LeftButton && scribbling&&(markTail)) {//Draw head dot
        drawMarkerDot(event->pos());
        scribbling = false;
    }
}

void PaintCanvas::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QRect dirtyRect = event->rect();
    painter.drawImage(dirtyRect, image, dirtyRect);
}
void PaintCanvas::drawMarkerDot(const QPoint &targetPoint)
{

    std::cout<<"DRAWMarkerDot MyPenWidth=  "<<myPenWidth<<std::endl;
    QPainter painter(&image);
    painter.setPen(QPen(myPenColor, myPenWidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    lastPoint.setX(targetPoint.x()+1);
    lastPoint.setY(targetPoint.y()+1);

    painter.drawLine(lastPoint, targetPoint);
    modified = true;
    int rad = (myPenWidth / 2) + 2;
    update(QRect(lastPoint, targetPoint).normalized()
           .adjusted(-rad, -rad, +rad, +rad));


    update();
}


void PaintCanvas::drawLineTo(const QPoint &endPoint)
{

//    std::cout<<"DRAWLINETO MyPenWidth=  "<<myPenWidth<<std::endl;
    QPainter painter(&image);
    painter.setPen(QPen(myPenColor, myPenWidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    painter.drawLine(lastPoint, endPoint);
    modified = true;
    int rad = (myPenWidth / 2) + 2;
    update(QRect(lastPoint, endPoint).normalized()
           .adjusted(-rad, -rad, +rad, +rad));

    //Path Test
    if(clickedToEndPath){
        QPainterPath invertPath;
        painter.setBrush(myBrushColor);

        invertPath.addRect(0,0,image.width(),image.height());
        invertPath.addPath(maskpath);

        painter.drawPath(invertPath);
    }
    //
    update();
    lastPoint = endPoint;
}

void PaintCanvas::resizeEvent(QResizeEvent *event)
{
//    if (width() > image.width() || height() > image.height()) {
        //Earlier they wanted a safe margine that the image would draw outside of hence the Qmax
        int newWidth = qMax(width() + 0, image.width());
        int newHeight = qMax(height() + 0, image.height());
        std::cout<<"RESIZEDEVENT  "<<newHeight<<std::endl;

        resizeImage(&image, QSize(newWidth, newHeight));
        update();
//    }
    QWidget::resizeEvent(event);
}



void PaintCanvas::resizeImage(QImage *image, const QSize &newSize)
{
    if (image->size() == newSize)
        return;

    QImage newImage(newSize, QImage::Format_ARGB32);
    newImage.fill(qRgba(255, 0, 255,0));
    QPainter painter(&newImage);
    *image=image->scaled(newSize);
    painter.drawImage(QPoint(0, 0), *image);
    *image = newImage;
    std::cout<<"RESIZEDIMAGE  "<<newSize.height()<<std::endl;


}

void PaintCanvas::print()
{
#ifndef QT_NO_PRINTER
    QPrinter printer(QPrinter::HighResolution);

    QPrintDialog *printDialog = new QPrintDialog(&printer, this);
    if (printDialog->exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = image.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(image.rect());
        painter.drawImage(0, 0, image);
    }
#endif // QT_NO_PRINTER
}
