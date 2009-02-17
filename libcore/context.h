/* This file is part of KCachegrind.
   Copyright (C) 2002 - 2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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

#ifndef CONTEXT_H
#define CONTEXT_H

#include <QString>
#include <QHash>

/**
 * Base class for source contexts which event costs contained
 * in a ProfileData instance, ie. a profiling experiment, can relate to.
 *
 * This only includes code/source related context. Any other cost
 * context such as thread number, see DataContext, which relates to ProfileData.
 */
class ProfileContext
{
public:
    // RTTI for trace item classes, using type() method
    enum Type {
	InvalidType, UnknownType,
	PartInstr, Instr,
	PartLine, Line,
	PartInstrJump, InstrJump,
	PartLineJump, LineJump,
	PartInstrCall, InstrCall,
	PartLineCall, LineCall,
	PartCall, Call,
	PartLineRegion, LineRegion,
	PartFunction, FunctionSource, Function, FunctionCycle,
	PartClass, Class, ClassCycle,
	PartFile, File, FileCycle,
	PartObject, Object, ObjectCycle,
	Part, Data,
	MaxType };

    ProfileContext(ProfileContext::Type);

    ProfileContext::Type type() { return _type; }

    static ProfileContext* context(ProfileContext::Type);

    // conversion of context type to locale independent string (e.g. for config)
    static QString typeName(Type);
    static Type type(const QString&);
    // the versions below should be used for user visible strings, as
    // these use localization settings
    static QString i18nTypeName(Type);
    static Type i18nType(const QString&);

    // clean up some static data
    static void cleanup();

private:
    Type _type;

    static QHash<QString, ProfileContext*> _contexts;
    static QString* _typeName;
    static QString* _i18nTypeName;
};

#endif // CONTEXT_H
