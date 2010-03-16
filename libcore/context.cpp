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

#include "context.h"

//---------------------------------------------------
// ProfileContext

QHash<QString, ProfileContext*> ProfileContext::_contexts;

QString* ProfileContext::_typeName = 0;
QString* ProfileContext::_i18nTypeName = 0;


ProfileContext::ProfileContext(ProfileContext::Type t)
{
	_type = t;
}

ProfileContext* ProfileContext::context(ProfileContext::Type t)
{
	QString key = QString("T%1").arg(t);
	if (_contexts.contains(key))
		return _contexts.value(key);

	ProfileContext* c = new ProfileContext(t);
	_contexts.insert(key, c);

	return c;
}

void ProfileContext::cleanup()
{
	if (_typeName) {
		delete [] _typeName;
		_typeName = 0;
	}
	if (_i18nTypeName) {
		delete [] _i18nTypeName;
		_i18nTypeName = 0;
	}

	qDeleteAll(_contexts);
	_contexts.clear();
}

QString ProfileContext::typeName(ProfileContext::Type t)
{
    if (!_typeName) {
	_typeName = new QString [MaxType+1];
	QString* strs = _typeName;
	for(int i=0;i<=MaxType;i++)
	    strs[i] = QString("?");

	strs[InvalidType] = QT_TR_NOOP("Invalid Context");
	strs[UnknownType] = QT_TR_NOOP("Unknown Context");
	strs[PartLine] = QT_TR_NOOP("Part Source Line");
	strs[Line] = QT_TR_NOOP("Source Line");
	strs[PartLineCall] = QT_TR_NOOP("Part Line Call");
	strs[LineCall] = QT_TR_NOOP("Line Call");
	strs[PartLineJump] = QT_TR_NOOP("Part Jump");
	strs[LineJump] = QT_TR_NOOP("Jump");
	strs[PartInstr] = QT_TR_NOOP("Part Instruction");
	strs[Instr] = QT_TR_NOOP("Instruction");
	strs[PartInstrJump] = QT_TR_NOOP("Part Instruction Jump");
	strs[InstrJump] = QT_TR_NOOP("Instruction Jump");
	strs[PartInstrCall] = QT_TR_NOOP("Part Instruction Call");
	strs[InstrCall] = QT_TR_NOOP("Instruction Call");
	strs[PartCall] = QT_TR_NOOP("Part Call");
	strs[Call] = QT_TR_NOOP("Call");
	strs[PartFunction] = QT_TR_NOOP("Part Function");
	strs[FunctionSource] = QT_TR_NOOP("Function Source File");
	strs[Function] = QT_TR_NOOP("Function");
	strs[FunctionCycle] = QT_TR_NOOP("Function Cycle");
	strs[PartClass] = QT_TR_NOOP("Part Class");
	strs[Class] = QT_TR_NOOP("Class");
	strs[PartFile] = QT_TR_NOOP("Part Source File");
	strs[File] = QT_TR_NOOP("Source File");
	strs[PartObject] = QT_TR_NOOP("Part ELF Object");
	strs[Object] = QT_TR_NOOP("ELF Object");
	strs[Part] = QT_TR_NOOP("Profile Part");
	strs[Data] = QT_TR_NOOP("Program Trace");
    }
    if (t<0 || t> MaxType) t = MaxType;
    return _typeName[t];
}

ProfileContext::Type ProfileContext::type(const QString& s)
{
    // This is the default context type
    if (s.isEmpty()) return Function;

    Type type;
    for (int i=0; i<MaxType;i++) {
	type = (Type) i;
	if (typeName(type) == s)
	    return type;
    }
    return UnknownType;
}

// all strings of typeName() are translatable because of QT_TR_NOOP there
QString ProfileContext::i18nTypeName(Type t)
{
    if (!_i18nTypeName) {
	_i18nTypeName = new QString [MaxType+1];
	for(int i=0;i<=MaxType;i++)
	    _i18nTypeName[i] = QObject::tr(typeName((Type)i).toUtf8());
    }
    if (t<0 || t> MaxType) t = MaxType;
    return _i18nTypeName[t];
}

ProfileContext::Type ProfileContext::i18nType(const QString& s)
{
    // This is the default context type
    if (s.isEmpty()) return Function;

    Type type;
    for (int i=0; i<MaxType;i++) {
	type = (Type) i;
	if (i18nTypeName(type) == s)
	    return type;
    }
    return UnknownType;
}
