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
 * Source annotation directory settings config page
 */

#include <QTreeWidgetItem>
#include <QFileDialog>

#include "tracedata.h"
#include "sourcesettings.h"
#include "globalconfig.h"

//
// SourceSettings
//

SourceSettings::SourceSettings(TraceData* data, QWidget* parent)
    : ConfigPage(parent,
		 QObject::tr("Source Annotation"),
		 QObject::tr("Directory Settings for Source Annotation"))
{
    ui.setupUi(this);

    ui.dirList->setRootIsDecorated(false);

    GlobalConfig* c = GlobalConfig::config();
    _always = tr("(always)");

    QTreeWidgetItem* i;
    QStringList::const_iterator sit = c->generalSourceDirs().constBegin();
    for(; sit != c->generalSourceDirs().constEnd(); ++sit ) {
      QString d = (*sit);
      if (d.isEmpty()) d = "/";
      i = new QTreeWidgetItem();
      i->setText(0, _always);
      i->setText(1, d);
      ui.dirList->addTopLevelItem(i);
    }

    QStringList objItems(_always);
    if (data) {
	TraceObjectMap::Iterator oit;
      for ( oit = data->objectMap().begin();
	    oit != data->objectMap().end(); ++oit ) {
	QString n = (*oit).name();
	if (n.isEmpty()) continue;
	objItems << n;

	const QStringList& dirs = c->objectSourceDirs(n);
	sit = dirs.constBegin();
	for(; sit != dirs.constEnd(); ++sit ) {
	  QString d = (*sit);
	  if (d.isEmpty()) d = "/";
	  i = new QTreeWidgetItem();
	  i->setText(0, n);
	  i->setText(1, d);
	  ui.dirList->addTopLevelItem(i);
	}
      }
    }

    ui.objectBox->addItems(objItems);
    ui.objectBox->setCurrentIndex(0);

    connect(ui.addDirButton, SIGNAL(clicked()),
	    this, SLOT(addClicked()));
    connect(ui.deleteDirButton, SIGNAL(clicked()),
	    this, SLOT(deleteClicked()));
    connect(ui.browseDirButton, SIGNAL(clicked()),
	    this, SLOT(browseClicked()));
    connect(ui.dirList,
	    SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
	    this,
	    SLOT(dirListItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui.objectBox,
	    SIGNAL(currentIndexChanged(QString)),
	    this, SLOT(objectChanged(QString)));
    connect(ui.dirEdit, SIGNAL(textChanged(QString)),
	    this, SLOT(dirEditChanged(QString)));

    _current = 0;
    update();
}

void SourceSettings::activate(QString s)
{
    int idx = s.toInt();
    if ((idx==0) || (idx>ui.dirList->topLevelItemCount())) return;

    ui.dirList->setCurrentItem(ui.dirList->topLevelItem(idx-1));
    ui.dirEdit->setFocus();
}

void SourceSettings::update()
{
    if (!_current) {
	ui.deleteDirButton->setEnabled(false);
	ui.objectBox->setEnabled(false);
	ui.dirEdit->setEnabled(false);
	ui.browseDirButton->setEnabled(false);
	return;
    }
    ui.deleteDirButton->setEnabled(true);
    ui.objectBox->setEnabled(true);
    ui.objectBox->setCurrentIndex(ui.objectBox->findText(_current->text(0)));
    ui.dirEdit->setEnabled(true);
    ui.dirEdit->setText(_current->text(1));
    ui.browseDirButton->setEnabled(true);
}

void SourceSettings::addClicked()
{
    QTreeWidgetItem* i = new QTreeWidgetItem();
    i->setText(0, ui.objectBox->currentText());
    i->setText(1, tr("<insert valid directory>"));
    ui.dirList->addTopLevelItem(i);
    ui.dirList->setCurrentItem(i);
}

void SourceSettings::deleteClicked()
{
    if (!_current) return;

    QTreeWidgetItem* i = _current;
    // the deletion removes the item
    delete _current;
    // deletion can trigger a call to dirListItemChanged() !
    if (_current == i) {
	_current = 0;
	update();
    }
}

void SourceSettings::browseClicked()
{
    QString d;
    d = QFileDialog::getExistingDirectory(this,
					  tr("Choose Source Directory"));
    if (!d.isEmpty())
	ui.dirEdit->setText(d);
}

void SourceSettings::dirListItemChanged(QTreeWidgetItem* current,
					QTreeWidgetItem*)
{
    _current = current;
    update();
}

void SourceSettings::objectChanged(QString obj)
{
    if (!_current) return;

    _current->setText(0, obj);
}

void SourceSettings::dirEditChanged(QString dir)
{
    if (!_current) return;

    _current->setText(1, dir);
}

bool SourceSettings::check(QString& errorMsg, QString& errorItem)
{
    for(int idx=0; idx< ui.dirList->topLevelItemCount(); idx++) {
	QTreeWidgetItem* item = ui.dirList->topLevelItem(idx);
	QString dir = item->text(1);
	if (QDir(dir).exists()) continue;
	errorMsg = tr("Directory does not exist");
	errorItem = QString("%1").arg(idx+1);
	return false;
    }
    return true;
}

void SourceSettings::accept()
{
    GlobalConfig* c = GlobalConfig::config();

    QHash<QString, QStringList> dirs;
    for(int idx=0; idx< ui.dirList->topLevelItemCount(); idx++) {
	QTreeWidgetItem* item = ui.dirList->topLevelItem(idx);
	dirs[item->text(0)] << item->text(1);
    }
    QHash<QString, QStringList>::const_iterator oit = dirs.constBegin();
    for(;oit != dirs.constEnd(); ++oit) {
	if (oit.key() == _always)
	    c->setGeneralSourceDirs(oit.value());
	else
	    c->setObjectSourceDirs(oit.key(), oit.value());
    }
}

#include "sourcesettings.moc"
