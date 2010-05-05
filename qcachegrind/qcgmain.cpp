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

#include <QDir>
#include <QApplication>

#include "qcgconfig.h"
#include "config.h"
#include "globalguiconfig.h"
#include "qcgtoplevel.h"
#include "tracedata.h"
#include "loader.h"

int main( int argc, char ** argv )
{
    QApplication app(argc, argv);
    QCGTopLevel* t;
    Loader::initLoaders();

    QCoreApplication::setOrganizationName("kcachegrind.sf.net");
    QCoreApplication::setApplicationName("QCachegrind");
    ConfigStorage::setStorage(new QCGConfigStorage);
    // creates global config object of type GlobalGUIConfig
    //GlobalGUIConfig::config()->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    if (!list.isEmpty()) {
	foreach(const QString& file, list) {
	    t = new QCGTopLevel();
	    t->show();
	    t->loadDelayed( QDir::fromNativeSeparators(file) );
	}
    }
    else {
	// load trace in current dir
	t = new QCGTopLevel();
	t->show();
	t->loadDelayed(".", false);
    }

    int res = app.exec();

    // to make leak checking in valgrind happy...
    Loader::deleteLoaders();
    ProfileContext::cleanup();
    ConfigStorage::cleanup();

    return res;
}
