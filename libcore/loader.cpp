/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2010 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

	_logger = 0;
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

	return 0;
}

Loader* Loader::loader(const QString& name)
{
	foreach (Loader* l, _loaderList)
		if (l->name() == name)
			return l;

	return 0;
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

