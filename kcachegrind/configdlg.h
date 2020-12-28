/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Configuration Dialog for KCachegrind
 */

#ifndef CONFIGDLG_H
#define CONFIGDLG_H

#include "ui_configdlgbase.h"

class TraceData;
class GlobalGUIConfig;
class ConfigColorSetting;

class ConfigDlgBase : public QDialog, public Ui::ConfigDlgBase
{
  Q_OBJECT
public:
    ConfigDlgBase( QWidget *parent ) : QDialog( parent ) {
        setupUi( this );
    }
};

class ConfigDlg : public ConfigDlgBase
{
    Q_OBJECT

public:
    ConfigDlg(GlobalGUIConfig*, TraceData*,
              QWidget* parent = nullptr);
    ~ConfigDlg() override;

    static bool configure(GlobalGUIConfig*, TraceData*, QWidget*);

protected Q_SLOTS:
    void objectActivated(const QString &);
    void objectCheckChanged(bool);
    void objectColorChanged(const QColor &);
    void classActivated(const QString &);
    void classCheckChanged(bool);
    void classColorChanged(const QColor &);
    void fileActivated(const QString &);
    void fileCheckChanged(bool);
    void fileColorChanged(const QColor &);
    void dirsItemChanged();
    void dirsDeletePressed();
    void dirsAddPressed();

private:
    QTreeWidgetItem *getSelectedDirItem();
    GlobalGUIConfig* _config;
    TraceData* _data;

    ConfigColorSetting *_objectCS, *_classCS, *_fileCS;
};

#endif
