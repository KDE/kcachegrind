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
 * KCachegrind startup
 */

// for KCACHEGRIND_VERSION
#include "../version.h"


#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "kdeconfig.h"
#include "toplevel.h"
#include "tracedata.h"
#include "loader.h"

int main( int argc, char ** argv )
{
  KAboutData aboutData("kcachegrind", 0,
                       ki18n("KCachegrind"),
                       KCACHEGRIND_VERSION,
                       ki18n("KDE Frontend for Callgrind/Cachegrind"),
                       KAboutData::License_GPL,
                       ki18n("(C) 2002 - 2011"), KLocalizedString(),
                       "http://kcachegrind.sf.net");
  aboutData.addAuthor(ki18n("Josef Weidendorfer"),
                      ki18n("Author/Maintainer"),
                      "Josef.Weidendorfer@gmx.de");

  KCmdLineArgs::init(argc, argv, &aboutData);

  KCmdLineOptions options;
  //options.add("r <exec>", ki18n("Run <exec> under cachegrind"));
  options.add("+[trace]", ki18n("Show information of this trace"));
  KCmdLineArgs::addCmdLineOptions( options );

  KApplication a;
  KGlobal::locale()->insertCatalog("kcachegrind_qt");
  TopLevel* t;
  Loader::initLoaders();

  KConfig* kc = KGlobal::config().data();
  ConfigStorage::setStorage(new KDEConfigStorage(kc));

  if (a.isSessionRestored()){
    int n = 1;
    while (KMainWindow::canBeRestored(n)){
      (new TopLevel())->restore(n);
      n++;
    }
  }
  else {
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    int nbArgs = args->count();
    if (nbArgs>0) {
      for(int i = 0; i < nbArgs; i++) {
        t = new TopLevel();
        t->show();
        t->loadDelayed(args->arg(i));
      }
    }
    else {
      // load trace in current dir
      t = new TopLevel();
      t->show();
      t->loadDelayed(".");
    }
  }

  a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );
  int res = a.exec();

  // to make leak checking in valgrind happy...
  Loader::deleteLoaders();
  ProfileContext::cleanup();
  ConfigStorage::cleanup();

  return res;
}
