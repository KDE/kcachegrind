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
    virtual ~ColorSettings();

    bool check(QString&, QString&);
    void accept();
    void activate(QString s);

public slots:
    void resetClicked();
    void colorListItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void colorChanged(const QColor &);

private:
    void update();

    Ui::ColorSettings ui;
    QTreeWidgetItem* _current;
};


#endif // COLORSETTINGS_H
