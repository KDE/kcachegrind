/* This file is part of KCachegrind.
   Copyright (c) 2008-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include <iostream>
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

void showHelp(bool fullHelp = true)
{
    std::cout <<  "Show profiles from callgrind files. (c) 2010-2016 J. Weidendorfer\n";

    if (!fullHelp)
        std::cout << "Type 'cgview -h' for help." << std::endl;
    else
        std::cout << "Usage: cgview [options] <file> ...\n\n"
               "Options:\n"
               " -h        Show this help text\n"
               " -e        Sort list according to exclusive cost\n"
               " -s <ev>   Sort and show counters for event <ev>\n"
               " -c        Sort by call count\n"
               " -b        Show butterfly (callers and callees)\n"
               " -n        Do not detect recursive cycles" << std::endl;

    exit(1);
}


int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    Loader::initLoaders();
    ConfigStorage::setStorage(new ConfigStorage);
    GlobalConfig::config()->addDefaultTypes();

    QStringList list = app.arguments();
    list.pop_front();
    if (list.isEmpty()) showHelp(false);

    bool sortByExcl = false;
    bool sortByCount = false;
    bool showCalls = false;
    QString showEvent;
    QStringList files;

    for(int arg = 0; arg<list.count(); arg++) {
        if      (list[arg] == QLatin1String("-h")) showHelp();
        else if (list[arg] == QLatin1String("-e")) sortByExcl = true;
        else if (list[arg] == QLatin1String("-n")) GlobalConfig::setShowCycles(false);
        else if (list[arg] == QLatin1String("-b")) showCalls = true;
        else if (list[arg] == QLatin1String("-c")) sortByCount = true;
        else if (list[arg] == QLatin1String("-s")) showEvent = list[++arg];
        else
            files << list[arg];
    }
    TraceData* d = new TraceData(new Logger);
    d->load(files);

    EventTypeSet* m = d->eventTypes();
    if (m->realCount() == 0) {
        std::cout << "Error: No event types found." << std::endl;
        return 1;
    }

    std::cout << "\nTotals for event types:\n";

    EventType* et;
    for (int i=0;i<m->realCount();i++) {
        et = m->realType(i);
        std::cout.setFieldWidth(14);
        std::cout.setFieldAlignment(QTextStream::AlignRight);
        std::cout << d->subCost(et).pretty();
        std::cout.setFieldWidth(0);
        std::cout << "   " << et->longName() << " (" << et->name() << ")\n";
    }
    for (int i=0;i<m->derivedCount();i++) {
        et = m->derivedType(i);
        std::cout.setFieldWidth(14);
        std::cout.setFieldAlignment(QTextStream::AlignRight);
        std::cout << d->subCost(et).pretty();
        std::cout.setFieldWidth(0);
        std::cout << "   " << et->longName() <<
                     " (" << et->name() << " = " << et->formula() << ")\n";
    }
    std::cout << endl;

    if (showEvent.isEmpty())
        et = m->realType(0);
    else {
        et = m->type(showEvent);
        if (!et) {
            std::cout << "Error: event '" << showEvent << "' not found." << endl;
            return 1;
        }
    }
    Q_ASSERT( et!=nullptr );
    std::cout << "Sorted by: " << (sortByExcl ? "Exclusive ":"Inclusive ")
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


    std::cout << "\n     Inclusive     Exclusive       Called  Function name (DSO)\n";
    std::cout << " ==================================================================\n";

    std::cout.setFieldAlignment(QTextStream::AlignRight);
    for(int i=0; i<hc.realCount(); i++) {
        f = (TraceFunction*)hc[i];

        if (showCalls) {
            if (i>0) std::cout << endl;
            foreach(TraceCall* c, f->callers()) {
                std::cout << "  ";
                std::cout.setFieldWidth(14);
                std::cout << c->subCost(et).pretty();
                std::cout.setFieldWidth(0);
                std::cout << "            ";
                std::cout.setFieldWidth(13);
                std::cout << c->prettyCallCount();
                std::cout.setFieldWidth(0);
                std::cout << "    < " << c->caller()->prettyName() << endl;
            }
        }

        std::cout.setFieldWidth(14);
        std::cout << f->inclusive()->subCost(et).pretty();
        std::cout << f->subCost(et).pretty();
        std::cout.setFieldWidth(13);
        std::cout << f->prettyCalledCount();
        std::cout.setFieldWidth(0);
        std::cout << "  " << f->name() << " (" << f->object()->name() << ")" << endl;

        if (showCalls) {
            foreach(TraceCall* c, f->callings()) {
                std::cout << "  ";
                std::cout.setFieldWidth(14);
                std::cout << c->subCost(et).pretty();
                std::cout.setFieldWidth(0);
                std::cout << "            ";
                std::cout.setFieldWidth(13);
                std::cout << c->prettyCallCount();
                std::cout.setFieldWidth(0);
                std::cout << "    > " << c->called()->prettyName() << endl;
            }
        }

    }
}

