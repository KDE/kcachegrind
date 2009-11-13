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
 * Base class of a subpage in the configuration dialog
 */

#include "configpage.h"

#include <QLabel>
#include <QWidget>

//
// ConfigPage
//

ConfigPage::ConfigPage(QWidget* parent, QString title, QString longTitle)
        : QWidget(parent)
{
    _title = title;
    if (longTitle.isEmpty())
        _longTitle = title;
    else
        _longTitle = longTitle;
}

void ConfigPage::activate(QString s)
{
    QWidget* w = _names.value(s);
    if (w) w->setFocus();
}

bool ConfigPage::check(QString&, QString&)
{
    return true;
}

void ConfigPage::accept()
{}

QString ConfigPage::inRangeError(int from, int to)
{
    return tr("Value must be between %1 and %2.").arg(from).arg(to);
}
