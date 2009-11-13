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
 * General settings config page
 */

#ifndef GENERALSETTINGS_H
#define GENERALSETTINGS_H

#include "configpage.h"
#include "ui_generalsettings.h"

class GeneralSettings: public ConfigPage
{
public:
    GeneralSettings(QWidget* parent);
    virtual ~GeneralSettings() {}

    bool check(QString&, QString&);
    void accept();

private:
    Ui::GeneralSettings ui;
};

#endif // GENERALSETTINGS_H
