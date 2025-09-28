/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2009-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Source annotation directory settings config page
 */

#include "sourcesettings.h"

#include <QTreeWidgetItem>
#include <QFileDialog>

#include "tracedata.h"
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
        if (d.isEmpty()) d = QStringLiteral("/");
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
                if (d.isEmpty()) d = QStringLiteral("/");
                i = new QTreeWidgetItem();
                i->setText(0, n);
                i->setText(1, d);
                ui.dirList->addTopLevelItem(i);
            }
        }
    }

    ui.objectBox->addItems(objItems);
    ui.objectBox->setCurrentIndex(0);

    connect(ui.addDirButton, &QAbstractButton::clicked,
            this, &SourceSettings::addClicked);
    connect(ui.deleteDirButton, &QAbstractButton::clicked,
            this, &SourceSettings::deleteClicked);
    connect(ui.browseDirButton, &QAbstractButton::clicked,
            this, &SourceSettings::browseClicked);
    connect(ui.dirList,
            &QTreeWidget::currentItemChanged,
            this,
            &SourceSettings::dirListItemChanged);
    connect(ui.objectBox,
            &QComboBox::currentIndexChanged,
            this, &SourceSettings::objectChanged);
    connect(ui.dirEdit, &QLineEdit::textChanged,
            this, &SourceSettings::dirEditChanged);

    _current = nullptr;
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
    int prevItemCount = ui.dirList->topLevelItemCount();
    QTreeWidgetItem* i = new QTreeWidgetItem();
    i->setText(0, ui.objectBox->currentText());
    i->setText(1, tr("<insert valid directory>"));
    ui.dirList->addTopLevelItem(i);
    ui.dirList->setCurrentItem(i);
    if (prevItemCount == 0 && ui.objectBox->currentText() == _always) {
        // it's not obvious that you have to click browse after adding an item,
        // but handle the case where we have no items and are looking for the
        // default. give users an opportunity to select for multiple ELF objects
        browseClicked();
    }
}

void SourceSettings::deleteClicked()
{
    if (!_current) return;

    QTreeWidgetItem* i = _current;
    // the deletion removes the item
    delete _current;
    // deletion can trigger a call to dirListItemChanged() !
    if (_current == i) {
        _current = nullptr;
        update();
    }
}

void SourceSettings::browseClicked()
{
    QString d;
    d = QFileDialog::getExistingDirectory(this,
                                          tr("Choose Source Directory"),
                                          ui.dirEdit->text());
    if (!d.isEmpty())
        ui.dirEdit->setText(d);
}

void SourceSettings::dirListItemChanged(QTreeWidgetItem* current,
                                        QTreeWidgetItem*)
{
    _current = current;
    update();
}

void SourceSettings::objectChanged(int objectIndex)
{
    if (!_current) return;

    const QString obj = ui.objectBox->itemText(objectIndex);
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
        errorItem = QStringLiteral("%1").arg(idx+1);
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

    c->setGeneralSourceDirs(QStringList());
    c->clearObjectSourceDirs();

    QHash<QString, QStringList>::const_iterator oit = dirs.constBegin();
    for(;oit != dirs.constEnd(); ++oit) {
        if (oit.key() == _always)
            c->setGeneralSourceDirs(oit.value());
        else
            c->setObjectSourceDirs(oit.key(), oit.value());
    }
}

#include "moc_sourcesettings.cpp"
