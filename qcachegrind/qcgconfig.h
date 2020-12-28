/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
