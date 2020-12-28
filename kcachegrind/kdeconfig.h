/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Configuration for KCachegrind
 */

#ifndef KDECONFIG_H
#define KDECONFIG_H

#include "config.h"

class KConfig;
class KConfigGroup;
class KDEConfigStorage;

class KDEConfigGroup: public ConfigGroup
{
    friend class KDEConfigStorage;

public:
    ~KDEConfigGroup() override;

    void setValue(const QString& key, const QVariant& value,
                  const QVariant& defaultValue = QVariant()) override;
    QVariant value(const QString& key, const QVariant& defaultValue) const override;

private:
    KDEConfigGroup(KConfigGroup*, bool);

    KConfigGroup* _kgroup;
    bool _readOnly;
};


class KDEConfigStorage : public ConfigStorage
{
public:
    KDEConfigStorage(KConfig*);

private:
    ConfigGroup* getGroup(const QString& group,
                          const QString& optSuffix) override;

    KConfig* _kconfig;
};

#endif
