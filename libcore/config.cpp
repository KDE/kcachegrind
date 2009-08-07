/* This file is part of KCachegrind.
   Copyright (C) 2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

ConfigStorage* ConfigStorage::_storage = 0;

ConfigStorage::ConfigStorage()
{
    _storage = 0;
}

ConfigStorage::~ConfigStorage()
{}

ConfigGroup* ConfigStorage::group(const QString& group,
				  const QString& optSuffix)
{
    Q_ASSERT(_storage != 0);

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
