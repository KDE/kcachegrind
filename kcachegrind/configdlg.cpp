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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Configuration Dialog for KCachegrind
 */

#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qdict.h>
#include <qmessagebox.h>

#include <kcolorbutton.h>
#include <kfiledialog.h>
#include <klocale.h>

#include "configdlg.h"
#include "tracedata.h"
#include "configuration.h"


ConfigDlg::ConfigDlg(Configuration* c, TraceData* data,
                     QWidget* parent, const char* name)
  :ConfigDlgBase(parent, name)
{
  _config = c;
  _data = data;
  _objectCS = 0;
  _classCS = 0;
  _fileCS = 0;

#if 0
  QListViewItem *oItem, *fItem, *cItem, *fnItem;
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

  QString objectPrefix = TraceCost::typeName(TraceCost::Object);
  QString classPrefix = TraceCost::typeName(TraceCost::Class);
  QString filePrefix = TraceCost::typeName(TraceCost::File);

  objectCombo->setDuplicatesEnabled(false);
  classCombo->setDuplicatesEnabled(false);
  fileCombo->setDuplicatesEnabled(false);
  objectCombo->setAutoCompletion(true);
  classCombo->setAutoCompletion(true);
  fileCombo->setAutoCompletion(true);

  // first unspecified cost items from data
  TraceObjectMap::Iterator oit;
  QStringList oList;
  for ( oit = data->objectMap().begin();
        oit != data->objectMap().end(); ++oit )
    oList.append((*oit).prettyName());

  TraceClassMap::Iterator cit;
  QStringList cList;
  for ( cit = data->classMap().begin();
        cit != data->classMap().end(); ++cit )
    cList.append((*cit).prettyName());

  TraceFileMap::Iterator fit;
  QStringList fList;
  for ( fit = data->fileMap().begin();
        fit != data->fileMap().end(); ++fit )
    fList.append((*fit).prettyName());

  // then already defined colors (have to check for duplicates!)
  QDictIterator<Configuration::ColorSetting> it( c->_colors );
  for( ; it.current(); ++it ) {
    if ((*it)->automatic) continue;

    QString n = it.currentKey();
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
  objectCombo->insertStringList(oList);
  classCombo->insertStringList(cList);
  fileCombo->insertStringList(fList);

  objectActivated(objectCombo->currentText());
  classActivated(classCombo->currentText());
  fileActivated(fileCombo->currentText());

  maxListEdit->setText(QString::number(c->_maxListCount));
  factorL1->setText(QString::number(c->_cycleL1Factor));
  factorL2->setText(QString::number(c->_cycleL2Factor));

  _dirItem = 0;

  QStringList::Iterator sit = c->_sourceDirs.begin();
  for(; sit != c->_sourceDirs.end(); ++sit ) {
    if ((*sit).isEmpty())
      dirList->insertItem("/");
    else
      dirList->insertItem(*sit);
  }

  connect(dirList, SIGNAL(currentChanged(QListBoxItem*)),
          this, SLOT(dirsItemChanged(QListBoxItem*)));
  connect(deleteDirButton, SIGNAL(pressed()),
          this, SLOT(dirsDeletePressed()));
  connect(addDirButton, SIGNAL(pressed()),
          this, SLOT(dirsAddPressed()));
  deleteDirButton->setEnabled(false);
  //subdirsCheck->setEnabled(false);

  symbolCount->setText(QString::number(c->_maxSymbolCount));
  symbolLength->setText(QString::number(c->_maxSymbolLength));
  precisionEdit->setText(QString::number(c->_percentPrecision));
}

ConfigDlg::~ConfigDlg()
{
}

bool ConfigDlg::configure(Configuration* c, TraceData* d, QWidget* p)
{
  ConfigDlg dlg(c, d, p);

  if (dlg.exec()) {

    bool ok;
    int max = dlg.maxListEdit->text().toUInt(&ok);
    if (ok && max < 500)
      c->_maxListCount = max;
    else
      QMessageBox::warning(p, i18n("KCachegrind Configuration"),
                           i18n("The Maximum Number of List Items should be below 500."
                                "The previous set value (%1) will still be used.")
                           .arg(QString::number(c->_maxListCount)),
                           QMessageBox::Ok, 0);

    int f1 = dlg.factorL1->text().toUInt(&ok);
    if (ok && f1<10000)
      c->_cycleL1Factor = f1;
    else
      QMessageBox::warning(p, i18n("KCachegrind Configuration"),
                           i18n("The L1 factor for the CPU Cycle Estimation "
                                "is not a valid. It has to be between "
                                "0 and 10000. The previous set "
                                "value (%1) will still be used.")
                           .arg(QString::number(c->_cycleL1Factor)),
                           QMessageBox::Ok, 0);

    int f2 = dlg.factorL2->text().toUInt(&ok);
    if (ok && f2<10000)
      c->_cycleL2Factor = f2;
    else
      QMessageBox::warning(p, i18n("KCachegrind Configuration"),
                           i18n("The L2 factor for the CPU Cycle Estimation "
                                "is not a valid. It has to be between "
                                "0 and 10000. The previous set "
                                "value (%1) will still be used.")
                           .arg(QString::number(c->_cycleL2Factor)),
                           QMessageBox::Ok, 0);

    c->_maxSymbolCount = dlg.symbolCount->text().toInt();
    c->_maxSymbolLength = dlg.symbolLength->text().toInt();
    c->_percentPrecision = dlg.precisionEdit->text().toInt();
    return true;
  }
  return false;
}

void ConfigDlg::objectActivated(const QString & s)
{
//  qDebug("objectActivated: %s", s.ascii());

  if (s.isEmpty()) { _objectCS=0; return; }

  QString n = TraceCost::typeName(TraceCost::Object) + "-" + s;

  Configuration* c = Configuration::config();
  Configuration::ColorSetting* cs = c->_colors[n];
  if (!cs)
    cs = Configuration::color(n);
//  else
//    qDebug("found color %s", n.ascii());

  _objectCS = cs;

  objectCheck->setChecked(cs->automatic);
  objectColor->setColor(cs->color);

  /*
  qDebug("Found Color %s, automatic to %s",
         _objectCS->name.ascii(),
         _objectCS->automatic ? "true":"false");
  */
}


void ConfigDlg::objectCheckChanged(bool b)
{
  if (_objectCS) {
    _objectCS->automatic = b;
    /*
    qDebug("Set Color %s automatic to %s",
           _objectCS->name.ascii(),
           _objectCS->automatic ? "true":"false");
    */
  }
}

void ConfigDlg::objectColorChanged(const QColor & c)
{
  if (_objectCS) _objectCS->color = c;
}

void ConfigDlg::classActivated(const QString & s)
{
//  qDebug("classActivated: %s", s.ascii());

  if (s.isEmpty()) { _classCS=0; return; }

  QString n = TraceCost::typeName(TraceCost::Class) + "-" + s;

  Configuration* c = Configuration::config();
  Configuration::ColorSetting* cs = c->_colors[n];
  if (!cs)
    cs = Configuration::color(n);

  _classCS = cs;

  classCheck->setChecked(cs->automatic);
  classColor->setColor(cs->color);

}


void ConfigDlg::classCheckChanged(bool b)
{
  if (_classCS) _classCS->automatic = b;
}

void ConfigDlg::classColorChanged(const QColor & c)
{
  if (_classCS) _classCS->color = c;
}


void ConfigDlg::fileActivated(const QString & s)
{
//  qDebug("fileActivated: %s", s.ascii());

  if (s.isEmpty()) { _fileCS=0; return; }

  QString n = TraceCost::typeName(TraceCost::File) + "-" + s;

  Configuration* c = Configuration::config();
  Configuration::ColorSetting* cs = c->_colors[n];
  if (!cs)
    cs = Configuration::color(n);

  _fileCS = cs;

  fileCheck->setChecked(cs->automatic);
  fileColor->setColor(cs->color);
}


void ConfigDlg::fileCheckChanged(bool b)
{
  if (_fileCS) _fileCS->automatic = b;
}

void ConfigDlg::fileColorChanged(const QColor & c)
{
  if (_fileCS) _fileCS->color = c;
}


void ConfigDlg::dirsItemChanged(QListBoxItem* i)
{
  _dirItem = i;
  deleteDirButton->setEnabled(i!=0);
  /*
  subdirsCheck->setEnabled(i!=0);
  if (i)
    subdirsCheck->setText(i18n("Include Subdirectories of %1")
                          .arg(i->text()));
  */
}

void ConfigDlg::dirsDeletePressed()
{
  if (_dirItem) {
    Configuration* c = Configuration::config();
    c->_sourceDirs.remove(_dirItem->text());

    delete _dirItem;
    _dirItem = 0;

    deleteDirButton->setEnabled(false);
  }
}

void ConfigDlg::dirsAddPressed()
{
  QString newDir;
  newDir = KFileDialog::getExistingDirectory(QString::null,
                                             this,
                                             i18n("Choose Source Directory"));
  if (newDir.isEmpty()) return;

  // even for "/", we strip the tailing slash
  if (newDir.endsWith("/"))
    newDir = newDir.left(newDir.length()-1);

  Configuration* c = Configuration::config();
  if (c->_sourceDirs.findIndex(newDir)>=0) return;

  c->_sourceDirs.append(newDir);
  if (newDir.isEmpty())
    dirList->insertItem("/");
  else
    dirList->insertItem(newDir);
}

#include "configdlg.moc"
