#include <QtGui/QApplication>
#include "modelmaker.h"
/***

Copyright 2012 Andrew Quitmeyer and Georgia Tech's Multi Agent Robotics and Systems Lab www.bio-tracking.org

    The files included with this piece of software are free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


***/


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ModelMaker w;
    w.show();

    return a.exec();
}
