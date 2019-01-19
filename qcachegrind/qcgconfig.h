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

#ifndef QCGCONFIG_H
#define QCGCONFIG_H

#include "config.h"

class QSettings;
class QCGConfigStorage;

class QCGConfigGroup: public ConfigGroup
{
    friend class QCGConfigStorage;

public:
    ~QCGConfigGroup() override;

    void setValue(const QString& key, const QVariant& value,
                  const QVariant& defaultValue = QVariant()) override;
    QVariant value(const QString& key, const QVariant& defaultValue) const override;

private:
    QCGConfigGroup(QSettings*, QString prefix, bool);

    QSettings* _settings;
    QString _prefix;
    bool _readOnly;
};


class QCGConfigStorage : public ConfigStorage
{
public:
    QCGConfigStorage();
    ~QCGConfigStorage() override;

private:
    ConfigGroup* getGroup(const QString& group,
                          const QString& optSuffix) override;

    QSettings* _settings;
};

#endif // QCGCONFIG_H
