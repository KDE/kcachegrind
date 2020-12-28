/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Base class for pages in configuration dialog
 */

#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QString>
#include <QMap>
#include <QWidget>

class ConfigPage: public QWidget
{
    Q_OBJECT
public:
    ConfigPage(QWidget* parent, QString title, QString longTitle = QString());
    ~ConfigPage() override {}

    QString title() { return _title; }
    QString longTitle() { return _longTitle; }

    // the default implementation focuses on widget named via <names>
    virtual void activate(QString);

    // called on OK; prohibits closing by returning false
    // an error message to show and item name to navigate to can be set
    virtual bool check(QString& errorMsg, QString& errorItem);
    virtual void accept();

protected:
    QString inRangeError(int, int);

    QMap<QString, QWidget*> _names;

private:
    QString _title;
    QString _longTitle;
};

#endif // CONFIGPAGE
