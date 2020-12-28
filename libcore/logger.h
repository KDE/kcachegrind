/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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


