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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * KCachegrind startup
 */

// for KCACHEGRIND_VERSION
#include "../version.h"

#include <qfile.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "toplevel.h"
#include "tracedata.h"
#include "loader.h"

static KCmdLineOptions options[] =
{
  { "r <exec>", I18N_NOOP("Run <exec> under cachegrind"), 0 },
  { "+[trace]", I18N_NOOP("Show information of this trace"), 0 },
  KCmdLineLastOption // End of options.
};

int main( int argc, char ** argv )
{
  KAboutData aboutData("kcachegrind",
                       I18N_NOOP("KCachegrind"),
                       KCACHEGRIND_VERSION,
                       I18N_NOOP("KDE Frontend for Cachegrind"),
                       KAboutData::License_GPL,
                       I18N_NOOP("(C) 2002, 2003, 2004"), 0,
		       "http://kcachegrind.sf.net");
  aboutData.addAuthor("Josef Weidendorfer",
                      I18N_NOOP("Author/Maintainer"),
                      "Josef.Weidendorfer@gmx.de");

  KCmdLineArgs::init(argc, argv, &aboutData);
  KCmdLineArgs::addCmdLineOptions( options );

  KApplication a;
  TopLevel* t;
  Loader::initLoaders();

  if (a.isRestored()){
    int n = 1;
    while (KMainWindow::canBeRestored(n)){
      (new TopLevel())->restore(n);
      n++;
    }
  }
  else {
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if (args->count()>0) {
      for(int i = 0; i < args->count(); i++) {
        t = new TopLevel();
        t->show();
        t->loadDelayed(QFile::decodeName(args->arg(i)));
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
  TraceItem::cleanup();

  return res;
}
