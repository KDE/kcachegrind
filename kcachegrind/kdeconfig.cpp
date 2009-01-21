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

KDEConfigGroup::KDEConfigGroup(const QString& group, KConfig* kconfig)
    : ConfigGroup(group)
{
    _kconfig = kconfig;
}

void KDEConfigGroup::setValue(const QString& key, const QVariant& value,
			      const QVariant& defaultValue)
{
    KConfigGroup c(_kconfig, _prefix + _group + _postfix);

    if (value == defaultValue) {
	c.deleteEntry(key);
	return;
    }

    _groupWrites++;

    switch(value.type()) {
	case QVariant::Bool:
	    c.writeEntry(key, value.toBool());
	    break;
	case QVariant::Int:
	    c.writeEntry(key, value.toInt());
	    break;
	case QVariant::Double:
	    c.writeEntry(key, value.toDouble());
	    break;
	case QVariant::String:
	    c.writeEntry(key, value.toString());
	    break;
	case QVariant::StringList:
	    c.writeEntry(key, value.toStringList());
	    break;
	case QVariant::Color:
	    c.writeEntry(key, value.value<QColor>());
	    break;
	default:
	    qFatal("KDEConfigGroup::setValue - QVariant type %s not supported",
		   value.typeName());
    }
}

QVariant KDEConfigGroup::value(const QString & key,
			       const QVariant & defaultValue) const
{
    KConfigGroup c(_kconfig, _prefix + _group + _postfix);

    switch(defaultValue.type()) {
	case QVariant::Bool:
	    return QVariant(c.readEntry(key, defaultValue.toBool()));
	case QVariant::Int:
	    return QVariant(c.readEntry(key, defaultValue.toInt()));
	case QVariant::Double:
	    return QVariant(c.readEntry(key, defaultValue.toDouble()));
	case QVariant::String:
	    return QVariant(c.readEntry(key, defaultValue.toString()));
	case QVariant::StringList:
	    return QVariant(c.readEntry(key, defaultValue.toStringList()));
	case QVariant::Color:
	    return QVariant(c.readEntry(key, defaultValue.value<QColor>()));
	default:
	    qFatal("KDEConfigGroup::value - QVariant type %s not supported",
		   defaultValue.typeName());
    }
    return QVariant();
}

void KDEConfigGroup::addSubGroup(const QString& prefix, const QString& postfix)
{
    ConfigGroup::addSubGroup(prefix, postfix);
}


void KDEConfigGroup::finishSubGroup()
{
    KConfigGroup c(_kconfig, _prefix + _group + _postfix);

    if (_groupWrites == 0)
	c.deleteEntry("subgroup");
    else
	c.writeEntry("subgroup", true);
}

bool KDEConfigGroup::hasSubGroup(const QString& prefix, const QString& optionalPostfix)
{
    KConfigGroup c(_kconfig, prefix + _group + optionalPostfix);
    if (c.readEntry("subgroup", false)) {
	ConfigGroup::addSubGroup(prefix, optionalPostfix);
	return true;
    }

    KConfigGroup c2(_kconfig, prefix + _group);
    if (c.readEntry("subgroup", false)) {
	ConfigGroup::addSubGroup(prefix, QString());
	return true;
    }

    return false;
}


//
// KDEConfigStorage
//

KDEConfigStorage::KDEConfigStorage(KConfig* kconfig)
{
    _kconfig = kconfig;
}

ConfigGroup* KDEConfigStorage::getGroup(const QString& group)
{
    return new KDEConfigGroup(group, _kconfig);
}
