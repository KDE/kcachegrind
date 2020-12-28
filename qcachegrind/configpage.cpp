/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
