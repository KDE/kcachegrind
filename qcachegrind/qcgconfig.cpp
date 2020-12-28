/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
