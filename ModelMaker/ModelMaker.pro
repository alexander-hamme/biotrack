#-------------------------------------------------
#
# Project created by QtCreator 2012-07-18T20:04:54
#
#-------------------------------------------------

QT       += core gui

TARGET = ModelMaker
TEMPLATE = app


SOURCES += main.cpp\
        modelmaker.cpp \
    paintcanvas.cpp

HEADERS  += modelmaker.h \
    paintcanvas.h

FORMS    += modelmaker.ui

INCLUDEPATH += /usr/include/opencv

#INCLUDEPATH += /usr/include/opencv-2.3.1

LIBS += -L/usr/local/lib -lopencv_core -lopencv_highgui


##use the below command when deploying

  QMAKE_LFLAGS += -Wl,--rpath=\\\$\$ORIGIN/libs
  QMAKE_LFLAGS_RPATH=


#Turn off the messages before you release!
    CONFIG += qt warn_off release
    DEFINES += QT_NO_DEBUG_OUTPUT
    DEFINES += QT_NO_DEBUG


unix {
        CONFIG += link_pkgconfig
        PKGCONFIG += opencv
}

RESOURCES += \
    modelmaker_res.qrc
