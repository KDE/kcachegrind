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
 * Color settings config page
 */

#include <QTreeWidgetItem>
#include <QFileDialog>

#include "tracedata.h"
#include "colorsettings.h"
#include "globalguiconfig.h"
#include "listutils.h"

//
// ColorSettings
//

static void insertColorItems(QTreeWidget* w, ProfileContext::Type type,
			QStringList items)
{
    items.sort();
    foreach(QString s, items) {
        ConfigColorSetting* cs = GlobalGUIConfig::groupColorSetting(type, s);
	QTreeWidgetItem* i = new QTreeWidgetItem(w);
	i->setText(0, ProfileContext::i18nTypeName(type));
	i->setData(0, Qt::UserRole, QVariant::fromValue((void*)cs));
	i->setIcon(1, QIcon(colorPixmap(20,10, cs->color())));
	i->setText(1, cs->automatic() ? QObject::tr("(auto)") : QString());
	i->setData(1, Qt::UserRole, cs->color());
	i->setText(2, s);
    }
}

ColorSettings::ColorSettings(TraceData* data, QWidget* parent)
    : ConfigPage(parent,
		 QObject::tr("Group Colors"),
		 QObject::tr("Color Settings for Function Groups"))
{
    ui.setupUi(this);

    ui.colorList->setRootIsDecorated(false);
    ui.colorList->setSortingEnabled(false);
    QStringList items;
    if (data) {
	TraceObjectMap::Iterator oit = data->objectMap().begin();
	for(; oit != data->objectMap().end(); ++oit)
	    items << (*oit).prettyName();
    }
    if (!items.contains(TraceObject::prettyEmptyName()))
	items << TraceObject::prettyEmptyName();
    insertColorItems(ui.colorList, ProfileContext::Object, items);
    items.clear();
    if (data) {
	TraceFileMap::Iterator fit = data->fileMap().begin();
	for(; fit != data->fileMap().end(); ++fit)
	    items << (*fit).prettyName();
    }
    if (!items.contains(TraceFile::prettyEmptyName()))
	items << TraceFile::prettyEmptyName();
    insertColorItems(ui.colorList, ProfileContext::File, items);
    items.clear();
    if (data) {
	TraceClassMap::Iterator cit = data->classMap().begin();
	for(; cit != data->classMap().end(); ++cit)
	    items << (*cit).prettyName();
    }
    if (!items.contains(TraceClass::prettyEmptyName()))
	items << TraceClass::prettyEmptyName();
    insertColorItems(ui.colorList, ProfileContext::Class, items);
    ui.colorList->setSortingEnabled(true);

    ui.colorList->resizeColumnToContents(0);
    ui.colorList->resizeColumnToContents(1);
    ui.colorList->resizeColumnToContents(2);

    connect(ui.resetButton, SIGNAL(clicked()),
	    this, SLOT(resetClicked()));
    connect(ui.colorList,
	    SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
	    this,
	    SLOT(colorListItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui.colorButton, SIGNAL(colorChanged(const QColor &)),
	    this, SLOT(colorChanged(const QColor &)));

    _current = 0;
    update();
}

ColorSettings::~ColorSettings()
{}

void ColorSettings::activate(QString s)
{
    int idx = s.toInt();
    if ((idx==0) || (idx>ui.colorList->topLevelItemCount())) return;

    ui.colorList->setCurrentItem(ui.colorList->topLevelItem(idx-1));
    ui.colorButton->setFocus();
}

void ColorSettings::update()
{
    if (!_current) {
	ui.resetButton->setEnabled(false);
	ui.colorButton->setEnabled(false);
	return;
    }
    ui.resetButton->setEnabled(true);
    ui.colorButton->setEnabled(true);

    QColor c = _current->data(1, Qt::UserRole).value<QColor>();
    ui.colorButton->setColor(c);
}

void ColorSettings::resetClicked()
{
    if (!_current) return;

    ConfigColorSetting*cs;
    cs = (ConfigColorSetting*) _current->data(0, Qt::UserRole).value<void*>();
    QColor c = cs->autoColor();
    _current->setIcon(1, QIcon(colorPixmap(20,10, c)));
    _current->setData(1, Qt::UserRole, c);
    _current->setText(1, QObject::tr("(auto)"));
    ui.colorButton->setColor(c);
}


void ColorSettings::colorListItemChanged(QTreeWidgetItem* current,
					 QTreeWidgetItem*)
{
    _current = current;
    update();
}

void ColorSettings::colorChanged(const QColor& c)
{
    if (!_current) return;

    _current->setIcon(1, QIcon(colorPixmap(20,10, c)));
    _current->setData(1, Qt::UserRole, c);
    _current->setText(1, QString());
}

bool ColorSettings::check(QString&, QString&)
{
    return true;
}

void ColorSettings::accept()
{
    QTreeWidgetItem* item;
    ConfigColorSetting* cs;
    QColor c;
    for(int i = 0; i< ui.colorList->topLevelItemCount(); i++) {
	item = ui.colorList->topLevelItem(i);
	cs = (ConfigColorSetting*) item->data(0, Qt::UserRole).value<void*>();
	c = item->data(1, Qt::UserRole).value<QColor>();
	if (cs->color() == c) continue;

	cs->setColor(c);
    }
}

#include "colorsettings.moc"
