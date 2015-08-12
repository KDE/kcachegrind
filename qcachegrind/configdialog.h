/* This file is part of KCachegrind.
   Copyright (c) 2009-2015 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * QCachegrind configuration dialog
 */

#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QString>
#include <QDialog>
#include <QMap>
#include <QTimer>

#include "configpage.h"

class QWidget;
class QLabel;
class QListWidget;
class QStackedWidget;

class TraceData;

class ConfigDialog: public QDialog
{
    Q_OBJECT

public:
    // If s is not empty, navigate to a given setting on opening
    ConfigDialog(TraceData* data, QWidget* parent, QString s = QString::null);

    void activate(QString);
    QString currentPage();

public slots:
    void accept();
    void listItemChanged(QString);
    void clearError();

private:
    void addPage(ConfigPage*);

    QLabel* _titleLabel;
    QLabel* _errorLabel;
    QListWidget *_listWidget;
    QStackedWidget *_widgetStack;
    QMap<QString,ConfigPage*> _pages;
    QString _activeSetting;
    QTimer _clearTimer;
};

#endif // CONFIGDIALOG
