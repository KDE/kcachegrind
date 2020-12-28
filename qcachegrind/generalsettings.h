/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
    Q_OBJECT
public:
    explicit GeneralSettings(QWidget* parent);
    ~GeneralSettings() override {}

    bool check(QString&, QString&) override;
    void accept() override;

private:
    Ui::GeneralSettings ui;
};

#endif // GENERALSETTINGS_H
