/* This file is part of KCachegrind.
   Copyright (C) 2002-2008 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

//
// ConfigGroup
//

ConfigGroup::ConfigGroup(const QString& group)
{
    _group = group;
}

ConfigGroup::~ConfigGroup()
{}

void ConfigGroup::setValue(const QString&, const QVariant&, const QVariant&)
{}

QVariant ConfigGroup::value(const QString&, const QVariant&) const
{
    return QVariant();
}

void ConfigGroup::addSubGroup(const QString& prefix, const QString& postfix)
{
    _prefix = prefix;
    _postfix = postfix;
    _groupWrites = 0;
}

void ConfigGroup::finishSubGroup()
{
    _prefix = QString::null;
    _postfix = QString::null;
}

bool ConfigGroup::hasSubGroup(const QString&, const QString&)
{
    return false;
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

ConfigGroup* ConfigStorage::group(const QString& group)
{
    Q_ASSERT(_storage != 0);

    return _storage->getGroup(group);
}

void ConfigStorage::setStorage(ConfigStorage* storage)
{
    _storage = storage;
}

ConfigGroup* ConfigStorage::getGroup(const QString& group)
{
    return new ConfigGroup(group);
}
