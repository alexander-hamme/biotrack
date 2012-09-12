#include <QtGui/QApplication>
#include <QFile>
#include <string>
#include <iostream>

#include "mainwindow.h"
#include "AbosThread.h"
#include "abospool.h"
#include "imagecapture.h"
#include "background.h"


int main(int argc, char *argv[])
{
    if(argc < 2){
        std::cout << "please specify an input video file." << std::endl;
        exit(0);
    }
	int fps = 30;
	// get ready the data pools
    AbosPool capture_pool, bg_pool;//, output_pool;
    capture_pool.clear();   // to make sure it is empty at first
	bg_pool.clear();
	//output_pool.clear();
	//init the three modules we wrote
    // image capture
    ImageCapture image_capture( fps );
    image_capture.setDeviceFile(argv[1]);   // read
    image_capture.setImagePool(&capture_pool);    // write
	//BG averaging
	Background bg(&capture_pool, &bg_pool, fps);
	//BG subtraction and thresholding
	//BGSubtract bgSub(&capture_pool, &bg_pool, &output_pool, 20,fps);
	// configure main window (QT)
    QVector<AbosThread*> thread_vec;
	QVector<AbosPool*> pool_vec;
	thread_vec.append(&bg);
	//thread_vec.append(&bgSub);
	thread_vec.append(&image_capture);
	pool_vec.append(&capture_pool);
	pool_vec.append(&bg_pool);
	//pool_vec.append(&output_pool);
    QApplication a(argc, argv);
    MainWindow w(&bg_pool, thread_vec, pool_vec, fps);
	w.startThreads(true);
    // start each threads
    w.show();
    // run main event loop of QT
    int ret = a.exec();
    // wait until every thread finish
    while ( image_capture.isRunning() || bg.isRunning() ) { }
	printf("Successfully terminated.\n");
    return ret;
}
