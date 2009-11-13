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

#include "generalsettings.h"
#include "globalconfig.h"

//
// GeneralSettings
//

GeneralSettings::GeneralSettings(QWidget* parent)
    : ConfigPage(parent,
                 QObject::tr("General"), QObject::tr("General Settings"))
{
    ui.setupUi(this);

    GlobalConfig* c = GlobalConfig::config();
    ui.maxListEdit->setText(QString::number(c->maxListCount()));
    ui.symbolCount->setText(QString::number(c->maxSymbolCount()));
    ui.symbolLength->setText(QString::number(c->maxSymbolLength()));
    ui.precisionEdit->setText(QString::number(c->percentPrecision()));
    ui.contextEdit->setText(QString::number(c->context()));

    _names.insert(QString("maxListEdit"), ui.maxListEdit);
    _names.insert(QString("symbolCount"), ui.symbolCount);
    _names.insert(QString("symbolLength"), ui.symbolLength);
    _names.insert(QString("precisionEdit"), ui.precisionEdit);
    _names.insert(QString("contextEdit"), ui.contextEdit);
}


bool GeneralSettings::check(QString& errorMsg, QString& errorItem)
{
    int v;
    v = ui.maxListEdit->text().toUInt();
    if ((v <1) || (v >500)) {
        errorMsg = inRangeError(1, 500);
        errorItem = QString("maxListEdit");
        return false;
    }

    v = ui.symbolCount->text().toInt();
    if ((v <1) || (v >50)) {
        errorMsg = inRangeError(1, 50);
        errorItem = QString("symbolCount");
        return false;
    }

    v = ui.symbolLength->text().toInt();
    if ((v <1) || (v >1000)) {
        errorMsg = inRangeError(1, 1000);
        errorItem = QString("symbolLength");
        return false;
    }

    v = ui.precisionEdit->text().toInt();
    if ((v <1) || (v >5)) {
        errorMsg = inRangeError(1, 5);
        errorItem = QString("precisionEdit");
        return false;
    }

    v = ui.contextEdit->text().toInt();
    if ((v <1) || (v >500)) {
        errorMsg = inRangeError(1, 500);
        errorItem = QString("contextEdit");
        return false;
    }

    return true;
}

void GeneralSettings::accept()
{
    GlobalConfig* c = GlobalConfig::config();

    c->setMaxListCount(ui.maxListEdit->text().toUInt());
    c->setMaxSymbolCount(ui.symbolCount->text().toInt());
    c->setMaxSymbolLength(ui.symbolLength->text().toInt());
    c->setPercentPrecision(ui.precisionEdit->text().toInt());
    c->setContext(ui.contextEdit->text().toInt());
}
