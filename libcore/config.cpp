/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2008-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include "config.h"

#include <QStringList>
#include <QList>

// helper functions

QList<int> toIntList(QStringList l)
{
    QList<int> iList;

    foreach(const QString& s, l)
        iList << s.toInt();

    return iList;
}

QStringList toStringList(QList<int> l)
{
    QStringList sList;

    foreach(int i, l)
        sList << QString::number(i);

    return sList;
}


//
// ConfigGroup
//

ConfigGroup::ConfigGroup()
{}

ConfigGroup::~ConfigGroup()
{}

void ConfigGroup::setValue(const QString&, const QVariant&, const QVariant&)
{}

QVariant ConfigGroup::value(const QString&, const QVariant& def) const
{
    return def;
}


//
// ConfigStorage
//

ConfigStorage* ConfigStorage::_storage = nullptr;

ConfigStorage::ConfigStorage()
{
    _storage = nullptr;
}

ConfigStorage::~ConfigStorage()
{}

ConfigGroup* ConfigStorage::group(const QString& group,
                                  const QString& optSuffix)
{
    Q_ASSERT(_storage != nullptr);

    return _storage->getGroup(group, optSuffix);
}

void ConfigStorage::setStorage(ConfigStorage* storage)
{
    _storage = storage;
}

void ConfigStorage::cleanup()
{
    delete _storage;
}

ConfigGroup* ConfigStorage::getGroup(const QString&, const QString&)
{
    return new ConfigGroup();
}
