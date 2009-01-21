/* This file is part of KCachegrind.
   Copyright (C) 2002-2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Dispatcher for messages (status/warning/errors) from libcore
 *
 * For different front ends which want to present the messages to the
 * user, a derived class should be implemented
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <qstring.h>
#include <qtimer.h>

class Logger
{
public:
    virtual ~Logger();

    // Notifications for file loading
    virtual void loadStart(const QString& filename);
    virtual void loadProgress(int progress); // 0 - 100
    virtual void loadWarning(int line, const QString& msg);
    virtual void loadError(int line, const QString& msg);
    virtual void loadFinished(const QString& msg); // msg could be error

protected:
    QString _filename;

private:
    QTimer _timer;
};

#endif // LOGGER_H


