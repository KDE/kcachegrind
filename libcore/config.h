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

#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QVariant>

class ConfigStorage;

// helper functions for storing specific values
QList<int> toIntList(QStringList l);
QStringList toStringList(QList<int> l);

/**
 * A group of configuration settings.
 * Actual implementation for the backends is done in derived classes.
 */
class ConfigGroup
{
    friend class ConfigStorage;

public:
    virtual ~ConfigGroup();

    // when value is defaultValue, any previous stored value is removed
    virtual void setValue(const QString& key, const QVariant& value,
			  const QVariant& defaultValue = QVariant());
    virtual QVariant value(const QString & key,
			   const QVariant& defaultValue) const;

    // the template version is needed for GUI Qt types
    template<typename T>
    inline T value(const QString & key,
		   const QVariant & defaultValue = QVariant()) const
    { return qVariantValue<T>( value(key, defaultValue) ); }

protected:
    ConfigGroup();
};

/**
 * This is an adapter class for different configuration backends.
 * A singleton.
 */

class ConfigStorage
{
public:
    ConfigStorage();
    virtual ~ConfigStorage();

    // if second parameter is not-empty, first search for an existing
    // group using the optional suffix, and then without.
    // the group gets readonly.
    static ConfigGroup* group(const QString& group,
			      const QString& optSuffix = QString());
    static void setStorage(ConfigStorage*);
    static void cleanup();

protected:
    virtual ConfigGroup* getGroup(const QString& group,
				  const QString& optSuffix);

    static ConfigStorage* _storage;
};


#endif // CONFIG_H
