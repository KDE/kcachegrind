/* This file is part of KCachegrind.
   Copyright (C) 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Base class for pages in configuration dialog
 */

#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QString>
#include <QMap>
#include <QWidget>

class ConfigPage: public QWidget
{
public:
    ConfigPage(QWidget* parent, QString title, QString longTitle = QString());
    virtual ~ConfigPage() {}

    QString title() { return _title; }
    QString longTitle() { return _longTitle; }

    // the default implementation focuses on widget named via <names>
    virtual void activate(QString);

    // called on OK; prohibits closing by returning false
    // an error message to show and item name to navigate to can be set
    virtual bool check(QString& errorMsg, QString& errorItem);
    virtual void accept();

protected:
    QString inRangeError(int, int);

    QMap<QString, QWidget*> _names;

private:
    QString _title;
    QString _longTitle;
};

#endif // CONFIGPAGE
