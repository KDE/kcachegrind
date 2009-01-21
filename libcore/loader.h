/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2007 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Base class for loaders of profiling data.
 */

#ifndef LOADER_H
#define LOADER_H

#include <QList>
#include <QString>

class QFile;
class TracePart;
class Loader;
class Logger;

/**
 * To implement a new loader, inherit from the Loader class and
 * and reimplement canLoadTrace() and loadTrace().
 *
 * For registration, put into the static initLoaders() function
 * of this base class a _loaderList.append(new MyLoader()).
 *
 * KCachegrind will use the first loader matching according to
 * canLoadTrace().
 *
 * To show progress and warnings while loading,
 *   loadStatus(), loadError() and loadWarning() should be called.
 * These are just shown as status, warnings or errors to the
 * user, but do not show real failure, as even errors can be
 * recoverable. For unablility to load a file, return false in
 * loadTrace().
 */

class Loader
{
public:
  Loader(const QString& name, const QString& desc);
  virtual ~Loader();

  // reimplement for a specific Loader
  virtual bool canLoadTrace(QFile* file);
  virtual bool loadTrace(TracePart*);

  static Loader* matchingLoader(QFile* file);
  static Loader* loader(const QString& name);
  static void initLoaders();
  static void deleteLoaders();

  QString name() const { return _name; }
  QString description() const { return _description; }

  // consumer for notifications
  void setLogger(Logger*);

protected:
  // notifications for the user
  void loadStart(const QString& filename);
  void loadProgress(int progress); // 0 - 100
  void loadError(int line, const QString& msg);
  void loadWarning(int line, const QString& msg);
  void loadFinished(const QString &msg = QString::null);

protected:
  Logger* _logger;

private:
  QString _name, _description;

  static QList<Loader*> _loaderList;
};


#endif
