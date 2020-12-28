/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
    ConfigDialog(TraceData* data, QWidget* parent, const QString &s = QString());

    void activate(QString);
    QString currentPage();

public Q_SLOTS:
    void accept() override;
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
