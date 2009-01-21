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

/*
 * Default implementation for notification dispatcher: use qDebug
 */

#include <QtDebug>

#include "logger.h"


/// Logger

Logger::~Logger()
{}

void Logger::loadStart(const QString& filename)
{
    _filename = filename;
    _timer.setSingleShot(true);
    _timer.start(1000);
    qDebug() << "Loading" << filename;
}

void Logger::loadProgress(int progress)
{
    // print progress at most every second
    if (_timer.isActive()) return;
    _timer.start(1000);

    qDebug() << "Loading" << _filename << "(" << progress << "%)";
}

void Logger::loadWarning(int line, const QString& msg)
{
    qDebug() << "Warning in " << _filename << ", line" << line
	     << ":" << msg;
}

void Logger::loadError(int line, const QString& msg)
{
    qDebug() << "Error in " << _filename << ", line" << line
	     << ":" << msg;
}

void Logger::loadFinished(const QString& msg)
{
    _timer.stop();
    if (msg.isEmpty())
	qDebug() << "File" << _filename << "loaded.";
    else
	qDebug() << "Error loading file" << _filename << ":" << qPrintable(msg);
}
