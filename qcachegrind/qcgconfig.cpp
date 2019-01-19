/* This file is part of KCachegrind.
   Copyright (c) 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#include "qcgconfig.h"

#include <QSettings>

#include "tracedata.h"
#include "traceitemview.h"

//
// QCGConfigGroup
//

QCGConfigGroup::QCGConfigGroup(QSettings* settings, QString prefix,
                               bool readOnly)
{
    _settings = settings;
    _prefix = prefix;
    _readOnly = readOnly;
}

QCGConfigGroup::~QCGConfigGroup()
{}

void QCGConfigGroup::setValue(const QString& key, const QVariant& value,
                              const QVariant& defaultValue)
{
    if ((_settings == nullptr) || _readOnly) return;

    QString fullKey = QStringLiteral("%1/%2").arg(_prefix).arg(key);
    if (value == defaultValue)
        _settings->remove(fullKey);
    else
        _settings->setValue(fullKey, value);
}

QVariant QCGConfigGroup::value(const QString& key,
                               const QVariant& defaultValue) const
{
    if (_settings == nullptr) return defaultValue;

    QString fullKey = QStringLiteral("%1/%2").arg(_prefix).arg(key);
    return _settings->value(fullKey, defaultValue);
}



//
// QCGConfigStorage
//

QCGConfigStorage::QCGConfigStorage()
{
    _settings = new QSettings;
}

QCGConfigStorage::~QCGConfigStorage()
{
    delete _settings;
}

ConfigGroup* QCGConfigStorage::getGroup(const QString& group,
                                        const QString& optSuffix)
{
    // for writing
    if (optSuffix.isEmpty())
        return new QCGConfigGroup(_settings, group, false);

    // for reading
    QStringList gList = _settings->childGroups();
    if (gList.contains(group+optSuffix))
        return new QCGConfigGroup(_settings, group + optSuffix, true);
    else if (gList.contains(group))
        return new QCGConfigGroup(_settings, group, true);

    // requested group does not exist, return only default values
    return new QCGConfigGroup(nullptr, QString(), true);
}
