/* This file is part of KCachegrind.
   Copyright (C) 2002 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Base class for loaders of profiling data.
 */

#ifndef LOADER_H
#define LOADER_H

#include <qobject.h>
#include <qptrlist.h>
#include <qstring.h>

class QFile;
class TraceData;
class TracePart;
class Loader;


typedef QPtrList<Loader> LoaderList;

/**
 * To implement a new loader, inherit from the Loader class
 * and implement canLoadTrace(), loadTrace() and if a trace in
 * this format can consist out of multiple parts, implement
 * isPartOfTrace(), too.
 * For registration, put into the static initLoaders() function
 * of this base class a _loaderList.append(new MyLoader()).
 *
 * KCachegrind will use the first matching loader.
 */

class Loader: public QObject
{
    Q_OBJECT

public:
  Loader(QString name, QString desc);
  virtual ~Loader();

  virtual bool canLoadTrace(QFile* file);
  virtual bool loadTrace(TracePart*);

  static Loader* matchingLoader(QFile* file);
  static Loader* loader(QString name);
  static void initLoaders();
  static void deleteLoaders();

  QString name() const { return _name; }
  QString description() const { return _description; }

signals:
  void updateStatus(QString, int);

private:
  QString _name, _description;

  static LoaderList _loaderList;
};



#endif
