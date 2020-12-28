/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2008-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Default implementation for notification dispatcher: use qDebug
 */

#include "logger.h"

#include <QtDebug>


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
