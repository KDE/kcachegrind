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
#include <Qt3Support/Q3ListView>
#include <Qt3Support/Q3Dict>

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
  KIntValidator * numValidator = new KIntValidator( this );
  maxListEdit->setValidator(numValidator );
  symbolCount->setValidator(numValidator );
  symbolLength->setValidator(numValidator );
  precisionEdit->setValidator(numValidator );
  contextEdit->setValidator(numValidator );

#if 0
  Q3ListViewItem *oItem, *fItem, *cItem, *fnItem;
  oItem = new(colorList, i18n("ELF Objects"));

  fItem = new(colorList, i18n("Source Files"));
  cItem = new(colorList, i18n("C++ Classes"));
  fnItem = new(colorList, i18n("Function (no Grouping)"));
#endif

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

  connect(PushButton2, SIGNAL(clicked()),SLOT(accept()));
  connect(PushButton1, SIGNAL(clicked()),SLOT(reject()));
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
      if (oList.findIndex(n) == -1) oList.append(n);
    }
    else if (n.startsWith(classPrefix)) {
      n = n.remove(0, classPrefix.length()+1);
      if (cList.findIndex(n) == -1) cList.append(n);
    }
    else if (n.startsWith(filePrefix)) {
      n = n.remove(0, filePrefix.length()+1);
      if (fList.findIndex(n) == -1) fList.append(n);
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

  maxListEdit->setText(QString::number(c->_maxListCount));

  _dirItem = 0;

  Q3ListViewItem* i = new Q3ListViewItem(dirList, i18n("(always)"));
  i->setOpen(true);
  QStringList::const_iterator sit = c->_generalSourceDirs.constBegin();
  for(; sit != c->_generalSourceDirs.constEnd(); ++sit ) {
    QString d = (*sit);
    if (d.isEmpty()) d = "/";
    new Q3ListViewItem(i, d);
  }
  if (data) {
    for ( oit = data->objectMap().begin();
          oit != data->objectMap().end(); ++oit ) {
      QString n = (*oit).name();
      if (n.isEmpty()) continue;
      i = new Q3ListViewItem(dirList, n);
      i->setOpen(true);
      QStringList dirs = c->_objectSourceDirs[n];

      sit = dirs.constBegin();
      for(; sit != dirs.constEnd(); ++sit ) {
        QString d = (*sit);
        if (d.isEmpty()) d = "/";
        new Q3ListViewItem(i, d);
      }
    }
  }

  connect(dirList, SIGNAL(selectionChanged(Q3ListViewItem*)),
          this, SLOT(dirsItemChanged(Q3ListViewItem*)));
  connect(addDirButton, SIGNAL(clicked()),
          this, SLOT(dirsAddPressed()));
  connect(deleteDirButton, SIGNAL(clicked()),
          this, SLOT(dirsDeletePressed()));
  dirList->setSelected(dirList->firstChild(), true);

  symbolCount->setText(QString::number(c->_maxSymbolCount));
  symbolLength->setText(QString::number(c->_maxSymbolLength));
  precisionEdit->setText(QString::number(c->_percentPrecision));
  contextEdit->setText(QString::number(c->_context));
}

ConfigDlg::~ConfigDlg()
{
}

bool ConfigDlg::configure(GlobalGUIConfig* c, TraceData* d, QWidget* p)
{
  ConfigDlg dlg(c, d, p);

  if (dlg.exec()) {

    bool ok;
    int newValue = dlg.maxListEdit->text().toUInt(&ok);
    if (ok && newValue < 500)
      c->_maxListCount = newValue;
    else
      QMessageBox::warning(p, i18n("KCachegrind Configuration"),
                           i18n("The Maximum Number of List Items should be below 500."
                                "The previous set value (%1) will still be used.",
                                c->_maxListCount),
                           QMessageBox::Ok, 0);

    c->_maxSymbolCount = dlg.symbolCount->text().toInt();
    c->_maxSymbolLength = dlg.symbolLength->text().toInt();
    c->_percentPrecision = dlg.precisionEdit->text().toInt();
    c->_context = dlg.contextEdit->text().toInt();
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


void ConfigDlg::dirsItemChanged(Q3ListViewItem* i)
{
  _dirItem = i;
  deleteDirButton->setEnabled(i->depth() == 1);
  addDirButton->setEnabled(i->depth() == 0);
}

void ConfigDlg::dirsDeletePressed()
{
  if (!_dirItem || (_dirItem->depth() == 0)) return;
  Q3ListViewItem* p = _dirItem->parent();
  if (!p) return;

  QString objName = p->text(0);

  QStringList* dirs;
  if (objName == i18n("(always)"))
    dirs = &(_config->_generalSourceDirs);
  else
    dirs = &(_config->_objectSourceDirs[objName]);

  dirs->removeAll(_dirItem->text(0));
  delete _dirItem;
  _dirItem = 0;

  deleteDirButton->setEnabled(false);
}

void ConfigDlg::dirsAddPressed()
{
  if (!_dirItem || (_dirItem->depth() >0)) return;

  QString objName = _dirItem->text(0);

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
  if (newDir.endsWith('/'))
    newDir = newDir.left(newDir.length()-1);

  if (dirs->findIndex(newDir)>=0) return;

  dirs->append(newDir);
  if (newDir.isEmpty()) newDir = QString("/");
  new Q3ListViewItem(_dirItem, newDir);
}

#include "configdlg.moc"
