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

#ifndef QTCONFIG_H
#define QTCONFIG_H

#include "config.h"

class QSettings;
class QtConfigStorage;

class QtConfigGroup: public ConfigGroup
{
    friend class QtConfigStorage;

public:
    ~QtConfigGroup();

    void setValue(const QString& key, const QVariant& value,
		  const QVariant& defaultValue = QVariant());
    QVariant value(const QString& key, const QVariant& defaultValue) const;

private:
    QtConfigGroup(QSettings*, bool);

    QSettings* _group;
    bool _readOnly;
};


class QtConfigStorage : public ConfigStorage
{
public:
  QtConfigStorage();

private:
  ConfigGroup* getGroup(const QString& group,
			const QString& optSuffix);
};

#endif
