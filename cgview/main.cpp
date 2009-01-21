/* This file is part of KCachegrind.
   Copyright (C) 2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <QCoreApplication>

#include "tracedata.h"
#include "loader.h"
#include "configuration.h"
#include "logger.h"

/*
 * Just a simple command line tool using libcore
 */

const char* pretty(const SubCost& c)
{
    return qPrintable(c.pretty(','));
}


int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    qDebug("cgview: display callgrind files. (C) 2008 J. Weidendorfer\n");

    Loader::initLoaders();
    Configuration* c = new Configuration();
    Configuration::setConfig(c);
    c->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    if (list.count() == 0) {
	qDebug("Usage: cgview file ...");
	return 1;
    }

    TraceData* d = new TraceData(new Logger);

    QString file;
    foreach(file, list)
	d->load(file);

    TraceEventTypeMapping* m = d->mapping();
    if (m->realCount() == 0) {
	qDebug("Error: No event types found.");
	return 1;
    }

    qDebug("\nTotals for event types:");

    TraceEventType* et;
    for (int i=0;i<m->realCount();i++) {
	et = m->realType(i);
	qDebug(" %12s   %s",
	       pretty(d->subCost(et)),
	       qPrintable(et->longName()));
    }
    for (int i=0;i<m->derivedCount();i++) {
	et = m->derivedType(i);
	qDebug(" %12s   %s (derived)",
	       pretty(d->subCost(et)),
	       qPrintable(et->longName()));
    }

    et = m->realType(0);
    Q_ASSERT( et!=0 );
    qDebug("\nFunction list sorted by '%s':", qPrintable(et->longName()));

    HighestCostList hc;
    hc.clear(50);
    TraceFunctionMap::Iterator it;
    for ( it = d->functionMap().begin();
	  it != d->functionMap().end(); ++it )
	hc.addCost(&(*it), (*it).inclusive()->subCost(et));

    TraceFunction *f;
    TraceFunctionCycleList l =  d->functionCycles();
    for (f=l.first();f;f=l.next())
	hc.addCost(f, f->inclusive()->subCost(et));

    for(int i=0;i<hc.realCount();i++) {
	f = (TraceFunction*)hc[i];
	qDebug(" %12s   %s (%s)",
	       pretty(f->inclusive()->subCost(et)),
	       qPrintable(f->name()),
	       qPrintable(f->object()->name()));
    }
}
