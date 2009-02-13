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
 * Configuration for QCachegrind
 */

#include "qtconfig.h"

#include <QSettings>

#include "tracedata.h"
#include "traceitemview.h"

//
// QtConfigGroup
//

QtConfigGroup::QtConfigGroup(QSettings* group, bool readOnly)
{
    _group = group;
    _readOnly = readOnly;
}

QtConfigGroup::~QtConfigGroup()
{
    delete _group;
}

void QtConfigGroup::setValue(const QString& key, const QVariant& value,
			     const QVariant& defaultValue)
{
    if ((_group == 0) || _readOnly) return;

    if (value == defaultValue)
	_group->remove(key);
    else
	_group->setValue(key, value);
}

QVariant QtConfigGroup::value(const QString& key,
			      const QVariant& defaultValue) const
{
    if (_group == 0) return defaultValue;

    return _group->value(key, defaultValue);
}



//
// QtConfigStorage
//

QtConfigStorage::QtConfigStorage()
{}

ConfigGroup* QtConfigStorage::getGroup(const QString& group,
				       const QString& optSuffix)
{
    QSettings* g = new QSettings;
    bool readOnly;

    if (!optSuffix.isEmpty()) {
	readOnly = true;
	QStringList gList = g->childGroups();
	if (gList.contains(group+optSuffix))
	    g->beginGroup(group+optSuffix);
	else if (gList.contains(group))
	    g->beginGroup(group);
	else {
	    delete g;
	    g = 0;
	}
    }
    else {
	readOnly = false;
	g->beginGroup(group);
    }

    return new QtConfigGroup(g, readOnly);
}
