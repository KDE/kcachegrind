/* This file is part of KCachegrind.
   Copyright (C) 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include <KColorButton>

#include "tracedata.h"
#include "globalguiconfig.h"


ConfigDlg::ConfigDlg(GlobalGUIConfig* c, TraceData* data,
                     QWidget* parent)
    :ConfigDlgBase(parent)
{
    _config = c;
    _data = data;
    _objectCS = nullptr;
    _classCS = nullptr;
    _fileCS = nullptr;

    connect(objectCombo, SIGNAL(activated(QString)),
            this, SLOT(objectActivated(QString)));
    connect(objectCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(objectActivated(QString)));
    connect(objectCheck, &QAbstractButton::toggled,
            this, &ConfigDlg::objectCheckChanged);
    connect(objectColor, &KColorButton::changed,
            this, &ConfigDlg::objectColorChanged);

    connect(classCombo, SIGNAL(activated(QString)),
            this, SLOT(classActivated(QString)));
    connect(classCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(classActivated(QString)));
    connect(classCheck, &QAbstractButton::toggled,
            this, &ConfigDlg::classCheckChanged);
    connect(classColor, &KColorButton::changed,
            this, &ConfigDlg::classColorChanged);

    connect(fileCombo, SIGNAL(activated(QString)),
            this, SLOT(fileActivated(QString)));
    connect(fileCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(fileActivated(QString)));
    connect(fileCheck, &QAbstractButton::toggled,
            this, &ConfigDlg::fileCheckChanged);
    connect(fileColor, &KColorButton::changed,
            this, &ConfigDlg::fileColorChanged);

    connect(buttonBox, &QDialogButtonBox::accepted,this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected,this, &QDialog::reject);
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

    connect(dirList, &QTreeWidget::itemSelectionChanged,
            this, &ConfigDlg::dirsItemChanged);
    connect(addDirButton, &QAbstractButton::clicked,
            this, &ConfigDlg::dirsAddPressed);
    connect(deleteDirButton, &QAbstractButton::clicked,
            this, &ConfigDlg::dirsDeletePressed);
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

    if (s.isEmpty()) { _objectCS=nullptr; return; }

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

    if (s.isEmpty()) { _classCS=nullptr; return; }

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

    if (s.isEmpty()) { _fileCS=nullptr; return; }

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
    return selectedItems.count() ? selectedItems[0] : nullptr;
    }

    void ConfigDlg::dirsItemChanged()
    {
    QTreeWidgetItem *dirItem = getSelectedDirItem();
    deleteDirButton->setEnabled(dirItem && dirItem->parent() != nullptr);
    addDirButton->setEnabled(dirItem && dirItem->parent() == nullptr);
}

void ConfigDlg::dirsDeletePressed()
{
    QTreeWidgetItem *dirItem = getSelectedDirItem();
    if (!dirItem || (dirItem->parent() == nullptr)) return;
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
    if (!dirItem || (dirItem->parent() != nullptr)) return;

    QString objName = dirItem->text(0);

    QStringList* dirs;
    if (objName == i18n("(always)"))
        dirs = &(_config->_generalSourceDirs);
    else
        dirs = &(_config->_objectSourceDirs[objName]);

    QString newDir;
    newDir = QFileDialog::getExistingDirectory(this,
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

