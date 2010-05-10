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
#include <QTextStream>

#include "tracedata.h"
#include "loader.h"
#include "config.h"
#include "globalconfig.h"
#include "logger.h"

/*
 * Just a simple command line tool using libcore
 */

void showHelp(QTextStream& out, bool fullHelp = true)
{
    out <<  "Show profiles from callgrind files. (C) 2010 J. Weidendorfer\n";

    if (!fullHelp)
        out << "Type 'cgview -h' for help." << endl;
    else
        out << "Usage: cgview [options] <file> ...\n\n"
            "Options:\n"
            " -h        Show this help text\n"
            " -e        Sort list according to exclusive cost\n"
            " -s <ev>   Sort and show counters for event <ev>\n"
            " -c        Sort by call count\n"
            " -b        Show butterfly (callers and callees)\n"
            " -n        Do not detect recursive cycles" << endl;

    exit(1);
}


int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    Loader::initLoaders();
    ConfigStorage::setStorage(new ConfigStorage);
    GlobalConfig::config()->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    if (list.isEmpty()) showHelp(out, false);

    bool sortByExcl = false;
    bool sortByCount = false;
    bool showCalls = false;
    QString showEvent;

    TraceData* d = new TraceData(new Logger);
    for(int arg = 0; arg<list.count(); arg++) {
        if      (list[arg] == "-h") showHelp(out);
        else if (list[arg] == "-e") sortByExcl = true;
        else if (list[arg] == "-n") GlobalConfig::setShowCycles(false);
        else if (list[arg] == "-b") showCalls = true;
        else if (list[arg] == "-c") sortByCount = true;
        else if (list[arg] == "-s") showEvent = list[++arg];
        else
            d->load(list[arg]);
    }

    EventTypeSet* m = d->eventTypes();
    if (m->realCount() == 0) {
        out << "Error: No event types found." << endl;
	return 1;
    }

    out << "\nTotals for event types:\n";

    QString p;
    EventType* et;
    for (int i=0;i<m->realCount();i++) {
	et = m->realType(i);
        out.setFieldWidth(14);
        out.setFieldAlignment(QTextStream::AlignRight);
        out << d->subCost(et).pretty();
        out.setFieldWidth(0);
        out << "   " << et->longName() << " (" << et->name() << ")\n";
    }
    for (int i=0;i<m->derivedCount();i++) {
	et = m->derivedType(i);
        out.setFieldWidth(14);
        out.setFieldAlignment(QTextStream::AlignRight);
        out << d->subCost(et).pretty();
        out.setFieldWidth(0);
        out << "   " << et->longName() <<
                " (" << et->name() << " = " << et->formula() << ")\n";
    }
    out << endl;

    if (showEvent.isEmpty())
        et = m->realType(0);
    else {
        et = m->type(showEvent);
        if (!et) {
            out << "Error: event '" << showEvent << "' not found." << endl;
            return 1;
        }
    }
    Q_ASSERT( et!=0 );
    out << "Sorted by: " << (sortByExcl ? "Exclusive ":"Inclusive ")
            << et->longName() << " (" << et->name() << ")" << endl;

    QList<TraceFunction*> flist;
    HighestCostList hc;
    hc.clear(50);
    TraceFunctionMap::Iterator it;
    for ( it = d->functionMap().begin(); it != d->functionMap().end(); ++it )
        flist.append(&(*it));

    TraceFunction *f;
    foreach(f, d->functionCycles())
        flist.append(f);

    foreach(f, flist) {
        if (sortByCount)
            hc.addCost(f, f->calledCount());
        else if (sortByExcl)
            hc.addCost(f, f->subCost(et));
        else
            hc.addCost(f, f->inclusive()->subCost(et));
    }


    out << "\n     Inclusive     Exclusive       Called  Function name (DSO)\n";
    out << " ==================================================================\n";

    out.setFieldAlignment(QTextStream::AlignRight);
    for(int i=0; i<hc.realCount(); i++) {
	f = (TraceFunction*)hc[i];

        if (showCalls) {
            if (i>0) out << endl;
            foreach(TraceCall* c, f->callers()) {
                out << "  ";
                out.setFieldWidth(14);
                out << c->subCost(et).pretty();
                out.setFieldWidth(0);
                out << "            ";
                out.setFieldWidth(13);
                out << c->prettyCallCount();
                out.setFieldWidth(0);
                out << "    < " << c->caller()->prettyName() << endl;
            }
        }

        out.setFieldWidth(14);
        out << f->inclusive()->subCost(et).pretty();
        out << f->subCost(et).pretty();
        out.setFieldWidth(13);
        out << f->prettyCalledCount();
        out.setFieldWidth(0);
        out << "  " << f->name() << " (" << f->object()->name() << ")" << endl;

        if (showCalls) {
            foreach(TraceCall* c, f->callings()) {
                out << "  ";
                out.setFieldWidth(14);
                out << c->subCost(et).pretty();
                out.setFieldWidth(0);
                out << "            ";
                out.setFieldWidth(13);
                out << c->prettyCallCount();
                out.setFieldWidth(0);
                out << "    > " << c->called()->prettyName() << endl;
            }
        }

    }
}

