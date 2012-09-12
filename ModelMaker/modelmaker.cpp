#include "modelmaker.h"
#include "ui_modelmaker.h"


ModelMaker::ModelMaker(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ModelMaker)
{
    ui->setupUi(this);
    ui->playButton->setEnabled(false);
    ui->seekbar->setEnabled(false);
    ui->subviewButton->setEnabled(false);
    paintCanvas=ui->paintwidget;
    paintCanvas->drawingPolygon = false;

    currentFrame =0;
    isPlaying=false;
    subtract =false;
    saveOut=false;
    isReady=false;
    checkReady();
    startTimer(0);

    ui->threshlabel->setText(QString::number(ui->threshbar->value()));


    //TODO make resizable

    ui->imagelabel->resize(ui->frame->width(),ui->frame->height());
    ui->paintwidget->resize(ui->frame->width(),ui->frame->height());
    subThreshold=ui->threshbar->value();
    //paintCanvas->resize(800,450);
    paintCanvas->resize(ui->frame->width(),ui->frame->height());


}

ModelMaker::~ModelMaker()
{
    capture.release();
    currentimg.release();
    delete ui;
}

void ModelMaker::checkReady()
{

    QString error="ERROR:  ";
    QString colour="red"; // you can use also QColor
    QString fonttemplate = "<font color='%1'>%2</font>";
    QString notprepared="<b>Not Ready!</b>  <i>Please choose a...</i> <br>";

    if (videopath==nopath)
        notprepared= notprepared+" ||   Video Source   ||   ";
    if (bgpath==nopath)
            notprepared= notprepared+"  ||    Background Image  ||  ";
    if (bgpath!=nopath && videopath!=nopath){
        colour="green"; // you can use also QColor
        QString text="<b>Ready to Make a Model</b> ";
        fonttemplate = tr("<font color='%1'>%2</font>");
        ui->messagelabel->setText(fonttemplate.arg( colour, text ));
        ui->seekbar->setEnabled(true);
        ui->subviewButton->setEnabled(true);
        ui->playButton->setEnabled(true);
        ui->playButton->setText("Play");
        ui->paintwidget->setToolTip("Left click to add mask points | Right click to close masking shape");
        paintCanvas->drawingPolygon = true;
        startpainter();
        capture.open(videopath.toStdString());
        bgimg = imread(bgpath.toStdString(),1);
        currentimg=bgimg.clone();
        displayImage(bgimg);
        current = QString::number(capture.get(CV_CAP_PROP_POS_FRAMES));
        isReady=true;
        ui->seekbar->setRange(0,capture.get(CV_CAP_PROP_FRAME_COUNT)-1);


        xscale=1.0;
                yscale=1.0;

            xscale=    capture.get(CV_CAP_PROP_FRAME_WIDTH)/ui->imagelabel->width();
            yscale=    capture.get(CV_CAP_PROP_FRAME_HEIGHT)/ui->imagelabel->height();

        ui->imagelabel->resize(ui->frame->width(),ui->frame->height());
        ui->paintwidget->resize(ui->frame->width(),ui->frame->height());


    }else{
        colour="red"; // you can use also QColor
        QString text=notprepared;
        fonttemplate = tr("<font color='%1'>%2</font>");
        ui->messagelabel->setText(fonttemplate.arg( colour, text ));
        everythingok=false;
    }
}


void ModelMaker::timerEvent(QTimerEvent*) {
    if(isReady){
        if(paintCanvas->hasClicked){
        }
        Mat img;

        QString constructText = "<b>Construct a polygon to make a model</b> <br> <font color='red'> <i color='red'>Left Click</i> </font> to draw points  <font color='green'> <i>Right Click</i> </font> to close path ";

        ui->messagelabel->setText(constructText);


        if (isPlaying && !isScrubbing && !paintCanvas->gettingCenter && !paintCanvas->gettingHead){
            if(currentFrame<capture.get(CV_CAP_PROP_FRAME_COUNT)){
                img = nextFrame();
                currentimg=img.clone();
                displayImage(img);
                current = QString::number(capture.get(CV_CAP_PROP_POS_FRAMES));
                ui->statusBar->showMessage(QString::number((int)(100*capture.get(CV_CAP_PROP_POS_FRAMES)/capture.get(CV_CAP_PROP_FRAME_COUNT)))+"%  "+
                                           current+"/"+(QString::number(capture.get(CV_CAP_PROP_FRAME_COUNT))));
                ui->seekbar->setValue(capture.get(CV_CAP_PROP_POS_FRAMES));

                currentFrame++;
           }
        }else if(isScrubbing){
            capture.read(img);
            if(subtract){
                img = subtractBack(img);
            }
            currentimg=img.clone();
            displayImage(img);
            isScrubbing=false;
           // isPlaying=true;
        }

        ///Determine the Center location of the target

        if(paintCanvas->gettingCenter){
            paintCanvas->lastPoint = QPoint(centroids.at(0).x()/xscale,centroids.at(0).y()/yscale);
            QString markcenterText = "<b>Mark Target Details</b> <br> <font color='red'> <i color='green'>Left Click</i> </font> to set the   <font color='red'> <b>CENTER</b> </font> of the target ";

            ui->messagelabel->setText(markcenterText);

        }

        if(paintCanvas->gettingHead){
            paintCanvas->lastPoint = QPoint(centroids.at(0).x()/xscale,centroids.at(0).y()/yscale);
            QString markheadText = "<b>Mark Target Details</b> <br> <font color='red'> <i color='green'>Left Click</i> </font> to set the   <font color='red'> <b>HEAD</b> (and thus, orientation) </font> of the target ";

            ui->messagelabel->setText(markheadText);
        }


        else if (saveOut&& !paintCanvas->gettingCenter&& !paintCanvas->gettingHead){
            capture.set(CV_CAP_PROP_POS_FRAMES,(double)currentFrame-1);
            capture.read(img);
            extractModel(img);
            capture.set(CV_CAP_PROP_POS_FRAMES,(double)currentFrame);

            saveOut=false;
        }

        if(paintCanvas->replyCenter == QMessageBox::Yes){
            paintCanvas->gettingCenter = false;

            //Start the "GetHead" method
            getHead();

            paintCanvas->replyCenter = replyNull;
            paintCanvas->clearImage();

        }
        else if(paintCanvas->replyCenter == QMessageBox::No){
            paintCanvas->clearImage();
            paintCanvas->gettingCenter = true;

        }



        if(paintCanvas->replyHead == QMessageBox::Yes){
            paintCanvas->gettingHead = false;

            double dX =-(paintCanvas->centerPoint.x()-paintCanvas->headPoint.x());
            double dY =-(paintCanvas->centerPoint.y()-paintCanvas->headPoint.y());

            angle =0.0;

            angle = atan2(dY, dX);


/*
            angle =0;
            double temp_angle =0;
            if(dX>0 && dY>0){
                temp_angle = atan(dY/dX);
                qDebug()<<"before"<<temp_angle*180/CV_PI;

                angle += CV_PI/2+temp_angle;

                qDebug()<<"++";
            }else if(dX<0 && dY<0){
                temp_angle = atan(dY/dX);
                qDebug()<<"before"<<temp_angle*180/CV_PI;
                temp_angle = CV_PI/2-temp_angle;

                angle += -temp_angle;
                qDebug()<<"--";
            }else  if(dX>0 && dY<0){
                temp_angle = atan(dY/dX);
                qDebug()<<"before"<<temp_angle*180/CV_PI;
                temp_angle = CV_PI/2+temp_angle;

                angle += temp_angle;
                qDebug()<<"+-";

            }else if(dX<0 && dY>0){
                temp_angle = atan(dY/dX);
                qDebug()<<"before"<<temp_angle*180/CV_PI;
                temp_angle = CV_PI/2-temp_angle;

                angle += -temp_angle;
                qDebug()<<"-+";

            }
            */
            qDebug()<<"after"<<angle*180/CV_PI;
            paintCanvas->replyHead = replyNull;
            paintCanvas->clearImage();

        }
        else if(paintCanvas->replyHead == QMessageBox::No){
            paintCanvas->clearImage();
            paintCanvas->gettingHead = true;

        }




        if(paintCanvas->replyMask == QMessageBox::Yes)
        {
            paintCanvas->polygons.push_back(paintCanvas->temp);
            bool ok;

            //Start the "Getcenter" method
            getCenter();
            QString text = QInputDialog::getText(this, "ROI Name", "Attach a name to this Region Of Interest", QLineEdit::Normal, "name",&ok);
            paintCanvas->polyNames.push_back(text);
            polyinfo = text;

            if(savepath==nopath){
                on_actionSave_Direcory_triggered();
            }

            processPolygons();

            saveOut =true;

            polyinfo += " Region added";
            ui->statusBar->showMessage(polyinfo,2000);
            paintCanvas->temp.clear();
            paintCanvas->replyMask = replyNull;


        } else if(paintCanvas->replyMask == QMessageBox::No){

            paintCanvas->masks.pop_back();
            polyinfo = "Polygon cleared";
            paintCanvas->temp.clear();
            ui->statusBar->showMessage(polyinfo,2000);
            paintCanvas->replyMask = replyNull;
        }


    }
}

void ModelMaker::displayImage(Mat cvimage)
{
    ui->imagelabel->resize(ui->frame->width(),ui->frame->height());
    ui->paintwidget->resize(ui->frame->width(),ui->frame->height());

    qimage = QImage((const uchar*)cvimage.data, cvimage.cols, cvimage.rows, QImage::Format_RGB888);
    qimage = qimage.rgbSwapped();
//    qimage = qimage.scaled(QSize(ui->frame->width(),ui->frame->height())


        qimage = qimage.scaled(QSize(ui->imagelabel->width(),ui->imagelabel->height())
                           //,Qt::KeepAspectRatio
                           );
//    qimage = qimage.scaled(QSize(800,450));

    ui->imagelabel->setPixmap(QPixmap::fromImage(qimage));
//    ui->imagelabel->picture()

        return;
}

Mat ModelMaker::nextFrame()
{
    Mat img;
    if (capture.get(CV_CAP_PROP_FRAME_COUNT)-1> capture.get(CV_CAP_PROP_POS_FRAMES))
    {
//        capture.retrieve(img);
        capture.read(img);
        if(subtract){
            img = subtractBack(img);
        }
    }
    else
    {
        img=currentimg.clone();
        //capture.set(CV_CAP_PROP_POS_FRAMES,capture.get(CV_CAP_PROP_FRAME_COUNT)-1);
    }

    return img;
}



void ModelMaker::on_actionLoad_Video_triggered()
{
    /* select a directory using file dialog */
    videopath = QFileDialog::getOpenFileName (this, tr("Open Video File"),lastpath, tr("Video (*.avi *.mov *.mpg *.mpeg *.mp4)"));

    if ( videopath.isNull() == false )
    {

        lastpath=videopath;
        lastpath.chop(4); // have to chop extension or else it overrides the filetype paramas


        ui->statusBar->showMessage("..."+videopath.right(50));
        qDebug()<<videopath;

    }else{
        videopath=nopath;
        ui->statusBar->showMessage(videopath.right(50));
    }

    checkReady();
}



void ModelMaker::on_actionLoad_Background_triggered()
{
    bgpath= QFileDialog::getOpenFileName(this, tr("Open Background File"),lastpath, tr("Images (*.png *.jpg)"));
    if ( bgpath.isNull() == false )
    {
     lastpath=bgpath;
     lastpath.chop(4); // have to chop extension or else it overrides the filetype paramas

     ui->statusBar->showMessage("..."+bgpath.right(50));
     checkReady();

    }
    else{
     bgpath=nopath;
     ui->statusBar->showMessage(bgpath.right(50));
    }
}


void ModelMaker::on_actionSave_Direcory_triggered()
{
    savepath = QFileDialog::getExistingDirectory(this, tr("Select a save directory"),lastpath);

    if ( savepath.isNull() == false )
    {
        ui->statusBar->showMessage("..."+savepath.right(50)+"/");
        lastpath=savepath;
        savepath=savepath+"/";
       // checkReady();
    }
    else{
        savepath=nopath;
        ui->statusBar->showMessage("defaultdirectory/");
    }
}

void ModelMaker::on_seekbar_valueChanged(int value)
{

}

void ModelMaker::on_playButton_clicked()
{
   isPlaying=!isPlaying;
   if(isPlaying){
       ui->playButton->setText("Pause");
   }else if(!isPlaying){
       ui->playButton->setText("Play");
   }
}

void ModelMaker::on_subviewButton_clicked(bool checked)
{
    subtract = checked;
    qDebug()<<subtract;
    if(checked){
        ui->subviewButton->setText("Normal");
    } else {
        ui->subviewButton->setText("Subtraction");
    }
}



void ModelMaker::startpainter()
{
    paintCanvas->backgroundmaskpen=true;
    QColor newColor = (QColor(150,255,150,150));

    paintCanvas->setPenColor(newColor);
    paintCanvas->setBrushColor(newColor);
    int newWidth=3;
    paintCanvas->setPenWidth(newWidth);

    ui->statusBar->showMessage(QString::fromStdString("Click to add points | Right click to close shape"),6000);
    maxX=0;
    QString constructText = "<b>Construct a polygon to make a model</b> <br> <font color='red'> <i color='red'>Left Click</i> </font> to draw points  <font color='green'> <i>Right Click</i> </font> to close path ";

    ui->messagelabel->setText(constructText);
}

void ModelMaker::processPolygons()
{

    x=0;
    y=0;
    avgX=0;
    avgY=0;
    xTot=0;
    yTot=0;
    maxY=0;
    minX=1000000;
    minY=1000000;

     xscale=1.0;
             yscale=1.0;

         xscale=    capture.get(CV_CAP_PROP_FRAME_WIDTH)/ui->imagelabel->width();
         yscale=    capture.get(CV_CAP_PROP_FRAME_HEIGHT)/ui->imagelabel->height();





    for (row = paintCanvas->polygons.begin(); row != paintCanvas->polygons.end(); ++row) {
        for (col = row->begin(); col != row->end(); ++col) {
            tempVariable =*row;
            tempPoint = *col;
            x = tempPoint.x();
            y = tempPoint.y();
            maxX = max(maxX,x);
            maxY = max(maxY,y);
            minX = min(minX,x);
            minY = min(minY,y);
            xTot += tempPoint.x();
            yTot += tempPoint.y();


            qDebug()<<"point: "<<x<<" , "<<y;
            qDebug()<<"total: "<<xTot<<" , "<<yTot;

            if(tempPoint==tempVariable.back()){
                avgX=xTot/tempVariable.size();
                avgY=yTot/tempVariable.size();
                tempCentroid.setX(avgX*xscale);
                tempCentroid.setY(avgY*yscale);
                centroids.push_back(tempCentroid);
                tempMax.setX(maxX*xscale);
                tempMax.setY(maxY*yscale);
                maxXY.push_back(tempMax);
                tempMin.setX(minX*xscale);
                tempMin.setY(minY*yscale);
                minXY.push_back(tempMin);
                qDebug()<<"Testing tempV.back(): "<<tempVariable.back();
                qDebug()<<"Testing tempV.size(): "<<tempVariable.size();
                qDebug()<<"avgX: "<<avgX*xscale<<" avgY: "<<avgY*yscale
                          ;
                qDebug()<<"max X Y: "<<maxXY.back()<<" min X Y: "<<minXY.back();


                tempCentroid.setX(avgX);
                tempCentroid.setY(avgY);

                xTot=0;
                yTot=0;
                maxX=0;
                maxY=0;
                minX=1000000;
                minY=1000000;

            }
        }
    }
}

cv::Mat ModelMaker::subtractBack(cv::Mat img)
{
//    absdiff(img, bgImage,bgSubImage);

//    cvtColor(bgSubImage,bgSubImageGray, CV_RGB2GRAY);// rgb -> gray //NOTE!!!! Never do a cvtColor(img,img, CVRGB2GRAY). if src and dst are same you get ERRORS!
//    cv::threshold(bgSubImageGray,bgSubImageGray,threshold,255,CV_THRESH_BINARY);



    Mat tempSubImg = img.clone();
    Mat SubImgMask = Mat(img.rows,img.cols,CV_8UC1);
    absdiff(img,bgimg,tempSubImg);

    cvtColor(tempSubImg,SubImgMask, CV_RGB2GRAY);
    cv::threshold(SubImgMask,SubImgMask,subThreshold,255,CV_THRESH_BINARY);

   // cv::bitwise_and(img,img,tempSubImg,tempSubImgGray);
    //img.copyTo(tempSubImg,SubImgMask);
     cvtColor(SubImgMask,tempSubImg,CV_GRAY2RGB);
     //tempSubImg = img-tempSubImg;
    return tempSubImg;
}

void ModelMaker::extractModel(cv::Mat origframe)
{
 //       qDebug()<<"Frame is"<<frame.empty();
        for(int i = 0; i < centroids.size(); i++) {

                Mat subtractedframe(origframe);
                subtractedframe = subtractBack(origframe);
                Mat polymask = subtractedframe.clone();

//                cv::cvtColor(frameMat, frameMat, CV_BGR2BGRA);

//                cv::cvtColor(polymask, polymask, CV_BGR2BGRA);


                //cv::rectangle(mask, Point( 0, 0 ), Point( mask.cols, mask.rows), Scalar( 0, 255,0,255 ),-1, 8 ); //Fill all mask in

                polymask.setTo(Scalar(0,0,0,0));
                //Polgon Masking
                polygon = paintCanvas->polygons.at(i);

                Point poly_points[polygon.size()];

                //Find point furthest from center
                Point furthest = Point(paintCanvas->centerPoint.x()*xscale,paintCanvas->centerPoint.y()*yscale);  //set to center

                int scaledcenterx = paintCanvas->centerPoint.x()*xscale;
                int scaledcentery = paintCanvas->centerPoint.y()*yscale;
                int scaledheadx= paintCanvas->headPoint.x()*xscale;
                int scaledheady=paintCanvas->headPoint.y()*yscale;


                float biggestdistancesquared=0;


                for(int j=0;j<polygon.size();j++)
                {
                    poly_points[j]=Point(xscale*polygon.at(j).x(), yscale*polygon.at(j).y());

                    Point candidate = Point(xscale*polygon.at(j).x(), yscale*polygon.at(j).y());
                    float distancecandidatesquared;
                    //Find furthest
                    distancecandidatesquared= (candidate.x - scaledcenterx)*(candidate.x - scaledcenterx) + (candidate.y - scaledcentery)*(candidate.y - scaledcentery);
                    if(distancecandidatesquared>biggestdistancesquared){
                        biggestdistancesquared=distancecandidatesquared;
                        qDebug()<<"biggcandidate x "<<candidate.x <<"  y "<<candidate.y << "    distance ="<<biggestdistancesquared;

                    }



                }

                const Point* ppt[1] = { poly_points };
                int npt[] = { polygon.size() };

                fillPoly( polymask,
                          ppt,
                          npt,
                          1,
                          Scalar( 255, 255,255, 255 ),
                          8,
                          0);



                //Debug
//                                cv::circle(frameMat,cv::Point(scaledcenterx,scaledcentery),3,Scalar(255,255,255),2);
//                                cv::circle(frameMat,cv::Point(scaledheadx,scaledheady),3,Scalar(255,0,255),2);
                //cv::circle(subtractedframe,cv::Point(scaledcenterx,scaledcentery),3,Scalar(255,255,0, 5),2);



                //background subtraction: take original image, apply background as a mask, save over original
                //bitwise_and(subtractedframe, polymask, subtractedframe);

                qDebug()<<"Roi "<<x1<<"  "<<y1<<"  "<<x2<<"  "<<y2<<"  ";

                //***Crop and Rotate ***//
                qDebug()<<"crop centered on  "<<scaledcenterx<<"  "<<scaledcentery;
                //crop the frame based on ROI
                Point2f src_center(scaledcenterx, scaledcentery);
                //To do this correctly use getRectSubPix instead of frameMat(MYROI) method

                getRectSubPix(origframe, cv::Size(sqrt(biggestdistancesquared)*2,sqrt(biggestdistancesquared)*2), src_center, origframe);
                getRectSubPix(subtractedframe, cv::Size(sqrt(biggestdistancesquared)*2,sqrt(biggestdistancesquared)*2), src_center, subtractedframe);
                getRectSubPix(polymask, cv::Size(sqrt(biggestdistancesquared)*2,sqrt(biggestdistancesquared)*2), src_center, polymask);

                //rotate the cropped frame about the center of the cropped frame.
                qDebug()<<"Rotate that image  "<<angle;
                origframe = rotateImage(origframe, angle);//Rotate full image about this center
                subtractedframe = rotateImage(subtractedframe, angle);//Rotate full image about this center
               polymask = rotateImage(polymask, angle);//Rotate full image about this center

                //after I rotate clear the global angle
                angle =0;
                //debug
                angle=-1;

cv::cvtColor(polymask,polymask, CV_RGB2GRAY);
//cv::copy(subtractedframe,subtractedframe,polymask);
Mat subtractedframenew;
subtractedframe.copyTo(subtractedframenew,polymask); // note that m.copyTo(m,mask) will have no masking effect

subtractedframe=subtractedframenew.clone();

                //Set Head to alpha=253
                //Set Center to Alpha = 254
                //Everything inside mask == alpha 255
                //Everything outside alpha=0;

               // Mat BGRa( 100, 100, CV_8UC4, Scalar(1,2,3,4) );
               // Mat bgr( BGRa.rows, BGRa.cols, CV_8UC3 );
               // Mat alpha( BGRa.rows, BGRa.cols, CV_8UC1 );
                Mat bgr;
                bgr=origframe.clone();

                Mat alpha;
                cvtColor(subtractedframe, alpha,CV_BGR2GRAY);

                Mat bgra;
                cvtColor(origframe, bgra,CV_BGR2BGRA);




                // forming array of matrices is quite efficient operations,
                // because the matrix data is not copied, only the headers
                Mat in[] = { bgr, alpha };
                // BGRa[0] -> bgr[0], BGRa[1] -> bgr[1],
                // BGRa[2] -> bgr[2], BGRa[3] -> alpha[0]
                int from_to[] = { 0,0,  1,1,  2,2,  3,3 };
                mixChannels( in, 2, &bgra, 1, from_to, 4 );


                QString ext = ".png";
//                QString modelfilename = savepath+paintCanvas->polyNames.at(i)+"_"+QString::number(centroids[i].x())+"_"+QString::number(centroids[i].y())+"_"+QString::number(currentFrame)+ext;
              QString modelfilename = savepath+paintCanvas->polyNames.at(i)+"_f"+QString::number(currentFrame)+ext;
                // imwrite(modelfilename.toStdString()+"_subtraction",subtractedframe);
               // imwrite(modelfilename.toStdString()+"_polymask",polymask);



                //save out Model
               //Cropped
               // imwrite(modelfilename.toStdString(),cropframe);
                //rotated
               imwrite(modelfilename.toStdString(),bgra);

                qDebug()<<"Saved out: "<<modelfilename;


                centroids.clear();
                maxXY.clear();
                minXY.clear();
                paintCanvas->polyNames.clear();
                paintCanvas->polygons.clear();
                paintCanvas->masks.pop_back();
                polyinfo = "Polygon cleared";
                paintCanvas->temp.clear();
                ui->statusBar->showMessage(polyinfo,2000);
                paintCanvas->replyMask = replyNull;
                capture.set(CV_CAP_PROP_POS_FRAMES,(double)currentFrame);
            }


}

void ModelMaker::getCenter()
{
    paintCanvas->gettingCenter =true;
}

void ModelMaker::getHead()
{
    paintCanvas->gettingHead =true;
}


//rotates about center
cv::Mat ModelMaker::rotateImage(const Mat& source, double anglerad)
{
    qDebug()<<"Angle in rad"<<anglerad;
    double angle  = ((anglerad*180)/CV_PI);
    qDebug()<<"Angle in deg"<<angle;
    Point2f src_center(source.cols/2.0F, source.rows/2.0F);
    Mat rot_mat = getRotationMatrix2D(src_center, angle, 1.0);
    Mat dst;
    warpAffine(source, dst, rot_mat, source.size());
    return dst;
}



//TODO implement a proper clear polygons clear everything


void ModelMaker::on_loadBackgroundpushButton_clicked()
{
    on_actionLoad_Background_triggered();
}

void ModelMaker::on_loadVideopushButton_clicked()
{
    on_actionLoad_Video_triggered();
}

void ModelMaker::on_seekbar_sliderMoved(int position)
{

Mat img;
currentFrame = position;
capture.set(CV_CAP_PROP_POS_FRAMES,(double)currentFrame);
current = QString::number(currentFrame);

qDebug()<<currentFrame;

ui->statusBar->showMessage(QString::number((int)(100*capture.get(CV_CAP_PROP_POS_FRAMES)/capture.get(CV_CAP_PROP_FRAME_COUNT)))+"%  "+
                           current+"/"+(QString::number(capture.get(CV_CAP_PROP_FRAME_COUNT))));

capture.read(img);
if(subtract){
    img = subtractBack(img);
}
currentimg=img.clone();
displayImage(img);
isScrubbing=false;
 isPlaying=false;
 ui->playButton->setText("Play");

}

void ModelMaker::resizeEvent(QResizeEvent *){
    ui->imagelabel->resize(ui->frame->width(),ui->frame->height());
    ui->paintwidget->resize(ui->frame->width(),ui->frame->height());
    paintCanvas->resizeImage(        &paintCanvas->image, QSize(ui->frame->width(),ui->frame->height()));
    displayImage(currentimg);
}


void ModelMaker::on_threshbar_sliderMoved(int position)
{
    QString lab = QString::number(position);
    ui->threshlabel->setText(lab);
subThreshold=position;
}