/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Configuration for KCachegrind
 */

#include "kdeconfig.h"

#include <KConfig>
#include <KConfigGroup>

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
    if ((_kgroup == nullptr) || _readOnly) return;

    if (value == defaultValue) {
        _kgroup->deleteEntry(key);
        return;
    }

    switch(value.typeId()) {
    case QMetaType::Bool:
        _kgroup->writeEntry(key, value.toBool());
        break;
    case QMetaType::Int:
        _kgroup->writeEntry(key, value.toInt());
        break;
    case QMetaType::Double:
        _kgroup->writeEntry(key, value.toDouble());
        break;
    case QMetaType::QString:
        _kgroup->writeEntry(key, value.toString());
        break;
    case QMetaType::QStringList:
        _kgroup->writeEntry(key, value.toStringList());
        break;
    case QMetaType::QColor:
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
    if (_kgroup == nullptr) return defaultValue;

    switch(defaultValue.typeId()) {
    case QMetaType::Bool:
        return QVariant(_kgroup->readEntry(key,
                                           defaultValue.toBool()));
    case QMetaType::Int:
        return QVariant(_kgroup->readEntry(key,
                                           defaultValue.toInt()));
    case QMetaType::Double:
        return QVariant(_kgroup->readEntry(key,
                                           defaultValue.toDouble()));
    case QMetaType::QString:
        return QVariant(_kgroup->readEntry(key,
                                           defaultValue.toString()));
    case QMetaType::QStringList:
        return QVariant(_kgroup->readEntry(key,
                                           defaultValue.toStringList()));
    case QMetaType::QColor:
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
            g = nullptr;
    }
    else {
        readOnly = false;
        g = new KConfigGroup(_kconfig, group);
    }

    return new KDEConfigGroup(g, readOnly);
}
