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
 * Configuration for KCachegrind
 */

#include "kdeconfig.h"

#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>

#include "tracedata.h"
#include "traceitemview.h"
#include "ui_configdlgbase.h"

//
// KDEConfigGroup
//

KDEConfigGroup::KDEConfigGroup(KConfigGroup* group, bool readOnly)
{
    _kgroup = group;
    _readOnly = readOnly;
}

KDEConfigGroup::~KDEConfigGroup()
{
    delete _kgroup;
}

void KDEConfigGroup::setValue(const QString& key, const QVariant& value,
			      const QVariant& defaultValue)
{
    if ((_kgroup == 0) || _readOnly) return;

    if (value == defaultValue) {
	_kgroup->deleteEntry(key);
	return;
    }

    switch(value.type()) {
	case QVariant::Bool:
	    _kgroup->writeEntry(key, value.toBool());
	    break;
	case QVariant::Int:
	    _kgroup->writeEntry(key, value.toInt());
	    break;
	case QVariant::Double:
	    _kgroup->writeEntry(key, value.toDouble());
	    break;
	case QVariant::String:
	    _kgroup->writeEntry(key, value.toString());
	    break;
	case QVariant::StringList:
	    _kgroup->writeEntry(key, value.toStringList());
	    break;
	case QVariant::Color:
	    _kgroup->writeEntry(key, value.value<QColor>());
	    break;
	default:
	    qFatal("KDEConfigGroup::setValue - QVariant type %s not supported",
		   value.typeName());
    }
}

QVariant KDEConfigGroup::value(const QString& key,
			       const QVariant& defaultValue) const
{
    if (_kgroup == 0) return defaultValue;

    switch(defaultValue.type()) {
	case QVariant::Bool:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.toBool()));
	case QVariant::Int:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.toInt()));
	case QVariant::Double:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.toDouble()));
	case QVariant::String:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.toString()));
	case QVariant::StringList:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.toStringList()));
	case QVariant::Color:
	    return QVariant(_kgroup->readEntry(key,
					       defaultValue.value<QColor>()));
	default:
	    qFatal("KDEConfigGroup::value - QVariant type %s not supported",
		   defaultValue.typeName());
    }
    return defaultValue;
}



//
// KDEConfigStorage
//

KDEConfigStorage::KDEConfigStorage(KConfig* kconfig)
{
    _kconfig = kconfig;
}

ConfigGroup* KDEConfigStorage::getGroup(const QString& group,
					const QString& optSuffix)
{
    KConfigGroup* g;
    bool readOnly;

    if (!optSuffix.isEmpty()) {
	readOnly = true;
	QStringList gList = _kconfig->groupList();
	if (gList.contains(group+optSuffix))
	    g = new KConfigGroup(_kconfig, group+optSuffix);
	else if (gList.contains(group))
	    g = new KConfigGroup(_kconfig, group);
	else
	    g = 0;
    }
    else {
	readOnly = false;
	g = new KConfigGroup(_kconfig, group);
    }

    return new KDEConfigGroup(g, readOnly);
}
