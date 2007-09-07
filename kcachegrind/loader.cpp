/* This file is part of KCachegrind.
   Copyright (C) 2002 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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


/// Loader

QList<Loader*> Loader::_loaderList;

Loader::Loader(QString name, QString desc)
{
	_name = name;
	_description = desc;
}

Loader::~Loader()
{}

bool Loader::canLoadTrace(QFile*)
{
	return false;
}

bool Loader::loadTrace(TracePart*)
{
	return false;
}

Loader* Loader::matchingLoader(QFile* file)
{
	foreach (Loader* l, _loaderList)
		if (l->canLoadTrace(file))
			return l;

	return 0;
}

Loader* Loader::loader(QString name)
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


#include "loader.moc"
