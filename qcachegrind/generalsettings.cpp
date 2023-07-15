/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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

    _names.insert(QStringLiteral("maxListEdit"), ui.maxListEdit);
    _names.insert(QStringLiteral("symbolCount"), ui.symbolCount);
    _names.insert(QStringLiteral("symbolLength"), ui.symbolLength);
    _names.insert(QStringLiteral("precisionEdit"), ui.precisionEdit);
    _names.insert(QStringLiteral("contextEdit"), ui.contextEdit);
}


bool GeneralSettings::check(QString& errorMsg, QString& errorItem)
{
    int v;
    v = ui.maxListEdit->text().toUInt();
    if ((v <1) || (v >500)) {
        errorMsg = inRangeError(1, 500);
        errorItem = QStringLiteral("maxListEdit");
        return false;
    }

    v = ui.symbolCount->text().toInt();
    if ((v <1) || (v >50)) {
        errorMsg = inRangeError(1, 50);
        errorItem = QStringLiteral("symbolCount");
        return false;
    }

    v = ui.symbolLength->text().toInt();
    if ((v <1) || (v >1000)) {
        errorMsg = inRangeError(1, 1000);
        errorItem = QStringLiteral("symbolLength");
        return false;
    }

    v = ui.precisionEdit->text().toInt();
    if ((v <1) || (v >5)) {
        errorMsg = inRangeError(1, 5);
        errorItem = QStringLiteral("precisionEdit");
        return false;
    }

    v = ui.contextEdit->text().toInt();
    if ((v <1) || (v >500)) {
        errorMsg = inRangeError(1, 500);
        errorItem = QStringLiteral("contextEdit");
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

#include "moc_generalsettings.cpp"
