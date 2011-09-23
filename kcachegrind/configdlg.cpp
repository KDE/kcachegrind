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

#include "configdlg.h"

#include <QCheckBox>
#include <QMessageBox>

#include <kcolorbutton.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <knumvalidator.h>

#include "tracedata.h"
#include "globalguiconfig.h"


ConfigDlg::ConfigDlg(GlobalGUIConfig* c, TraceData* data,
                     QWidget* parent)
  :ConfigDlgBase(parent)
{
  _config = c;
  _data = data;
  _objectCS = 0;
  _classCS = 0;
  _fileCS = 0;

  connect(objectCombo, SIGNAL(activated(const QString &)),
          this, SLOT(objectActivated(const QString &)));
  connect(objectCombo, SIGNAL(textChanged(const QString &)),
          this, SLOT(objectActivated(const QString &)));
  connect(objectCheck, SIGNAL(toggled(bool)),
          this, SLOT(objectCheckChanged(bool)));
  connect(objectColor, SIGNAL(changed(const QColor &)),
          this, SLOT(objectColorChanged(const QColor &)));

  connect(classCombo, SIGNAL(activated(const QString &)),
          this, SLOT(classActivated(const QString &)));
  connect(classCombo, SIGNAL(textChanged(const QString &)),
          this, SLOT(classActivated(const QString &)));
  connect(classCheck, SIGNAL(toggled(bool)),
          this, SLOT(classCheckChanged(bool)));
  connect(classColor, SIGNAL(changed(const QColor &)),
          this, SLOT(classColorChanged(const QColor &)));

  connect(fileCombo, SIGNAL(activated(const QString &)),
          this, SLOT(fileActivated(const QString &)));
  connect(fileCombo, SIGNAL(textChanged(const QString &)),
          this, SLOT(fileActivated(const QString &)));
  connect(fileCheck, SIGNAL(toggled(bool)),
          this, SLOT(fileCheckChanged(bool)));
  connect(fileColor, SIGNAL(changed(const QColor &)),
          this, SLOT(fileColorChanged(const QColor &)));

  connect(buttonBox, SIGNAL(accepted()),SLOT(accept()));
  connect(buttonBox, SIGNAL(rejected()),SLOT(reject()));
  QString objectPrefix = ProfileContext::typeName(ProfileContext::Object);
  QString classPrefix = ProfileContext::typeName(ProfileContext::Class);
  QString filePrefix = ProfileContext::typeName(ProfileContext::File);

  objectCombo->setDuplicatesEnabled(false);
  classCombo->setDuplicatesEnabled(false);
  fileCombo->setDuplicatesEnabled(false);
  objectCombo->setAutoCompletion(true);
  classCombo->setAutoCompletion(true);
  fileCombo->setAutoCompletion(true);

  // first unspecified cost items from data
  TraceObjectMap::Iterator oit;
  QStringList oList;
  if (data) {
    for ( oit = data->objectMap().begin();
          oit != data->objectMap().end(); ++oit )
      oList.append((*oit).prettyName());
  }

  TraceClassMap::Iterator cit;
  QStringList cList;
  if (data) {
    for ( cit = data->classMap().begin();
          cit != data->classMap().end(); ++cit )
      cList.append((*cit).prettyName());
  }

  TraceFileMap::Iterator fit;
  QStringList fList;
  if (data) {
    for ( fit = data->fileMap().begin();
          fit != data->fileMap().end(); ++fit )
      fList.append((*fit).prettyName());
  }

  // then already defined colors (have to check for duplicates!)
  foreach(ConfigColorSetting* cs, c->_colors) {
    if (cs->_automatic) continue;

    QString n = cs->_name;
    if (n.startsWith(objectPrefix)) {
      n = n.remove(0, objectPrefix.length()+1);
      if (oList.indexOf(n) == -1) oList.append(n);
    }
    else if (n.startsWith(classPrefix)) {
      n = n.remove(0, classPrefix.length()+1);
      if (cList.indexOf(n) == -1) cList.append(n);
    }
    else if (n.startsWith(filePrefix)) {
      n = n.remove(0, filePrefix.length()+1);
      if (fList.indexOf(n) == -1) fList.append(n);
    }
  }

  oList.sort();
  cList.sort();
  fList.sort();
  objectCombo->addItems(oList);
  classCombo->addItems(cList);
  fileCombo->addItems(fList);

  objectActivated(objectCombo->currentText());
  classActivated(classCombo->currentText());
  fileActivated(fileCombo->currentText());

  maxListEdit->setValue(c->_maxListCount);

  QTreeWidgetItem* i = new QTreeWidgetItem(dirList);
  i->setText(0, i18n("(always)"));
  i->setExpanded(true);
  QTreeWidgetItem* root = i;
  QStringList::const_iterator sit = c->_generalSourceDirs.constBegin();
  for(; sit != c->_generalSourceDirs.constEnd(); ++sit ) {
    QTreeWidgetItem *item = new QTreeWidgetItem(i);
    item->setText(0, sit->isEmpty() ? QDir::rootPath() : *sit);
  }
  if (data) {
    for ( oit = data->objectMap().begin();
          oit != data->objectMap().end(); ++oit ) {
      const QString n = (*oit).name();
      if (n.isEmpty()) continue;
      i = new QTreeWidgetItem(dirList);
      i->setText(0, n);
      i->setExpanded(true);
      const QStringList dirs = c->_objectSourceDirs[n];

      sit = dirs.constBegin();
      for(; sit != dirs.constEnd(); ++sit ) {
        QTreeWidgetItem *item = new QTreeWidgetItem(i);
        item->setText(0, sit->isEmpty() ? QDir::rootPath() : *sit);
      }
    }
  }

  connect(dirList, SIGNAL(itemSelectionChanged()),
          this, SLOT(dirsItemChanged()));
  connect(addDirButton, SIGNAL(clicked()),
          this, SLOT(dirsAddPressed()));
  connect(deleteDirButton, SIGNAL(clicked()),
          this, SLOT(dirsDeletePressed()));
  dirList->setCurrentItem(root);

  symbolCount->setValue(c->_maxSymbolCount);
  symbolLength->setValue(c->_maxSymbolLength);
  precisionEdit->setValue(c->_percentPrecision);
  contextEdit->setValue(c->_context);
}

ConfigDlg::~ConfigDlg()
{
}

bool ConfigDlg::configure(GlobalGUIConfig* c, TraceData* d, QWidget* p)
{
  ConfigDlg dlg(c, d, p);

  if (dlg.exec()) {

    // max 499 per definition
    c->_maxListCount = dlg.maxListEdit->value();
    c->_maxSymbolCount = dlg.symbolCount->value();
    c->_maxSymbolLength = dlg.symbolLength->value();
    c->_percentPrecision = dlg.precisionEdit->value();
    c->_context = dlg.contextEdit->value();
    return true;
  }
  return false;
}

void ConfigDlg::objectActivated(const QString & s)
{
//  qDebug("objectActivated: %s", s.ascii());

  if (s.isEmpty()) { _objectCS=0; return; }

  QString n = ProfileContext::typeName(ProfileContext::Object) + '-' + s;

  ConfigColorSetting* cs = _config->_colors[n];
  if (!cs)
    cs = GlobalGUIConfig::colorSetting(n);
//  else
//    qDebug("found color %s", n.ascii());

  _objectCS = cs;

  objectCheck->setChecked(cs->_automatic);
  objectColor->setColor(cs->_color);

  /*
  qDebug("Found Color %s, automatic to %s",
         _objectCS->name.ascii(),
         _objectCS->automatic ? "true":"false");
  */
}


void ConfigDlg::objectCheckChanged(bool b)
{
  if (_objectCS) {
    _objectCS->_automatic = b;
    /*
    qDebug("Set Color %s automatic to %s",
           _objectCS->name.ascii(),
           _objectCS->automatic ? "true":"false");
    */
  }
}

void ConfigDlg::objectColorChanged(const QColor & c)
{
  if (_objectCS) _objectCS->_color = c;
}

void ConfigDlg::classActivated(const QString & s)
{
//  qDebug("classActivated: %s", s.ascii());

  if (s.isEmpty()) { _classCS=0; return; }

  QString n = ProfileContext::typeName(ProfileContext::Class) + '-' + s;

  ConfigColorSetting* cs = _config->_colors[n];
  if (!cs)
    cs = GlobalGUIConfig::colorSetting(n);

  _classCS = cs;

  classCheck->setChecked(cs->_automatic);
  classColor->setColor(cs->_color);

}


void ConfigDlg::classCheckChanged(bool b)
{
  if (_classCS) _classCS->_automatic = b;
}

void ConfigDlg::classColorChanged(const QColor & c)
{
  if (_classCS) _classCS->_color = c;
}


void ConfigDlg::fileActivated(const QString & s)
{
//  qDebug("fileActivated: %s", s.ascii());

  if (s.isEmpty()) { _fileCS=0; return; }

  QString n = ProfileContext::typeName(ProfileContext::File) + '-' + s;

  ConfigColorSetting* cs = _config->_colors[n];
  if (!cs)
    cs = GlobalGUIConfig::colorSetting(n);

  _fileCS = cs;

  fileCheck->setChecked(cs->_automatic);
  fileColor->setColor(cs->_color);
}


void ConfigDlg::fileCheckChanged(bool b)
{
  if (_fileCS) _fileCS->_automatic = b;
}

void ConfigDlg::fileColorChanged(const QColor & c)
{
  if (_fileCS) _fileCS->_color = c;
}

QTreeWidgetItem *ConfigDlg::getSelectedDirItem()
{
  const QList<QTreeWidgetItem*> selectedItems = dirList->selectedItems();
  return selectedItems.count() ? selectedItems[0] : NULL;
}

void ConfigDlg::dirsItemChanged()
{
  QTreeWidgetItem *dirItem = getSelectedDirItem();
  deleteDirButton->setEnabled(dirItem && dirItem->parent() != NULL);
  addDirButton->setEnabled(dirItem && dirItem->parent() == NULL);
}

void ConfigDlg::dirsDeletePressed()
{
  QTreeWidgetItem *dirItem = getSelectedDirItem();
  if (!dirItem || (dirItem->parent() == 0)) return;
  QTreeWidgetItem* p = dirItem->parent();
  if (!p) return;

  QString objName = p->text(0);

  QStringList* dirs;
  if (objName == i18n("(always)"))
    dirs = &(_config->_generalSourceDirs);
  else
    dirs = &(_config->_objectSourceDirs[objName]);

  dirs->removeAll(dirItem->text(0));
  delete dirItem;

  deleteDirButton->setEnabled(false);
}

void ConfigDlg::dirsAddPressed()
{
  QTreeWidgetItem *dirItem = getSelectedDirItem();
  if (!dirItem || (dirItem->parent() != 0)) return;

  QString objName = dirItem->text(0);

  QStringList* dirs;
  if (objName == i18n("(always)"))
    dirs = &(_config->_generalSourceDirs);
  else
    dirs = &(_config->_objectSourceDirs[objName]);

  QString newDir;
  newDir = KFileDialog::getExistingDirectory(KUrl(),
                                             this,
                                             i18n("Choose Source Folder"));
  if (newDir.isEmpty()) return;

  // even for '/', we strip the tailing slash
  if (newDir.endsWith(QLatin1Char('/')))
    newDir = newDir.left(newDir.length()-1);

  if (dirs->indexOf(newDir)>=0) return;

  dirs->append(newDir);
  if (newDir.isEmpty()) newDir = QDir::rootPath();
  QTreeWidgetItem *item = new QTreeWidgetItem(dirItem);
  item->setText(0, newDir);
}

#include "configdlg.moc"
