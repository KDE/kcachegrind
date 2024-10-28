/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2002-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
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
        InvalidType = 0, UnknownType,
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
        BasicBlock, Branch,
        MaxType};

    explicit ProfileContext(ProfileContext::Type = InvalidType);

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

    static ProfileContext* _contexts;
    static QString* _typeName;
    static QString* _i18nTypeName;
};

#endif // CONTEXT_H
