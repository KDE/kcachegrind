/* This file is part of KCachegrind.
   Copyright (C) 2002, 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
            QWidget* parent = 0);
  ~ConfigDlg();

  static bool configure(GlobalGUIConfig*, TraceData*, QWidget*);

protected slots:
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
