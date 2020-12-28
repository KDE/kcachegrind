/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Color settings config page
 */

#ifndef COLORSETTINGS_H
#define COLORSETTINGS_H

#include "configpage.h"
#include "context.h"
#include "ui_colorsettings.h"

class TraceData;
class QTreeWidgetItem;

class ColorSettings: public ConfigPage
{
    Q_OBJECT

public:
    ColorSettings(TraceData* data, QWidget* parent);
    ~ColorSettings() override;

    bool check(QString&, QString&) override;
    void accept() override;
    void activate(QString s) override;

public Q_SLOTS:
    void resetClicked();
    void colorListItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void colorChanged(const QColor &);

private:
    void update();

    Ui::ColorSettings ui;
    QTreeWidgetItem* _current;
};


#endif // COLORSETTINGS_H
