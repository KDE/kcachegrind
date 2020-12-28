/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * Base class for loaders of profiling data.
 */

#include "loader.h"

#include "logger.h"

/// Loader

QList<Loader*> Loader::_loaderList;

Loader::Loader(const QString& name, const QString& desc)
{
    _name = name;
    _description = desc;

    _logger = nullptr;
}

Loader::~Loader()
{}

bool Loader::canLoad(QIODevice*)
{
    return false;
}

int Loader::load(TraceData*, QIODevice*, const QString&)
{
    return 0;
}

Loader* Loader::matchingLoader(QIODevice* file)
{
    foreach (Loader* l, _loaderList)
        if (l->canLoad(file))
            return l;

    return nullptr;
}

Loader* Loader::loader(const QString& name)
{
    foreach (Loader* l, _loaderList)
        if (l->name() == name)
            return l;

    return nullptr;
}

// factories of available loaders
Loader* createCachegrindLoader();

void Loader::initLoaders()
{
    _loaderList.append(createCachegrindLoader());
    //_loaderList.append(GProfLoader::createLoader());
}

void Loader::deleteLoaders()
{
    while (!_loaderList.isEmpty())
        delete _loaderList.takeFirst();
}

// notifications

void Loader::setLogger(Logger* l)
{
    _logger = l;
}

void Loader::loadStart(const QString& filename)
{
    if (_logger)
        _logger->loadStart(filename);
}

void Loader::loadProgress(int progress)
{
    if (_logger)
        _logger->loadProgress(progress);
}

void Loader::loadError(int line, const QString& msg)
{
    if (_logger)
        _logger->loadError(line, msg);
}

void Loader::loadWarning(int line, const QString& msg)
{
    if (_logger)
        _logger->loadWarning(line, msg);
}

void Loader::loadFinished(const QString& msg)
{
    if (_logger)
        _logger->loadFinished(msg);
}

