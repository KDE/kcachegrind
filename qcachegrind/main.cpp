/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

   KCachegrind is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * QCachegrind startup
 */

#include <QFile>
#include <QApplication>

#include "qtconfig.h"
#include "config.h"
#include "globalconfig.h"
#include "toplevel.h"
#include "tracedata.h"
#include "loader.h"

int main( int argc, char ** argv )
{
    QApplication app(argc, argv);
    TopLevel* t;
    Loader::initLoaders();

    QCoreApplication::setOrganizationName("kcachegrind.sf.net");
    QCoreApplication::setOrganizationDomain("kcachegrind.sf.net");
    QCoreApplication::setApplicationName("QCachegrind");
    ConfigStorage::setStorage(new QtConfigStorage);
    GlobalConfig::config()->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    QString file;
    if (!list.isEmpty()) {
	foreach(file, list) {
	    t = new TopLevel();
	    t->show();
	    t->loadDelayed(file);
	}
    }
    else {
	// load trace in current dir
	t = new TopLevel();
	t->show();
	t->loadDelayed(".");
    }

    return app.exec();
}
