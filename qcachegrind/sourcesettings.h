/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Source annotation directory settings config page
 */

#ifndef SOURCESETTINGS_H
#define SOURCESETTINGS_H

#include "configpage.h"
#include "ui_sourcesettings.h"

class TraceData;
class QTreeWidgetItem;

class SourceSettings: public ConfigPage
{
    Q_OBJECT

public:
    SourceSettings(TraceData* data, QWidget* parent);
    ~SourceSettings() override {}

    bool check(QString&, QString&) override;
    void accept() override;
    void activate(QString s) override;

public Q_SLOTS:
    void addClicked();
    void deleteClicked();
    void browseClicked();
    void dirListItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void objectChanged(QString);
    void dirEditChanged(QString);

private:
    void update();

    Ui::SourceSettings ui;
    QTreeWidgetItem* _current;
    QString _always;
};


#endif // SOURCESETTINGS_H
