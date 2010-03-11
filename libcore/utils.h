/* This file is part of KCachegrind.
   Copyright (C) 2003 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

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
 * Utility classes for KCachegrind
 */

#ifndef UTILS_H
#define UTILS_H

#include <qstring.h>

class QIODevice;

typedef unsigned long long uint64;
typedef long long int64;

/**
 * A simple, constant string class
 *
 * For use with zero-copy strings from mapped files.
 */
class FixString {

 public:
    // constructor for an invalid string
    FixString() { _len = 0; _str = 0; }

    /**
     * FixString never does a deep copy! You have to make sure that
     * the string starting at the char pointer is valid trough the
     * lifetime of FixString.
     */
    FixString(const char*, int len);

    int len() { return _len; }
    const char* ascii() { return _str; }
    bool isEmpty() { return _len == 0; }
    bool isValid() { return _str != 0; }

    // sets <c> to first character and returns true if length >0
    bool first(char& c)
	{ if (_len==0) return false; c=_str[0]; return true; }

    void set(const char* s, int l) { _str=s; _len=l; }
    bool stripFirst(char&);
    bool stripPrefix(const char*);

    /**
     * Strip leading and trailing spaces
     */
    void stripSurroundingSpaces();

    /**
     * Strip leading spaces
     */
    void stripSpaces();

    /**
     * Strip name: [A-Za-z_][0-9A_Za-z_]*
     */
    bool stripName(FixString&);

    /**
     * Strip string until char appears or end. Strips char, too.
     */
    FixString stripUntil(char);

    bool stripUInt(uint&, bool stripSpaces = true);
    bool stripUInt64(uint64&, bool stripSpaces = true);
    bool stripInt64(int64&, bool stripSpaces = true);

    operator QString() const
	{ return QString::fromLatin1(_str,_len); }

 private:
    const char* _str;
    int _len;
};


/**
 * A class for fast line by line reading of a read-only ASCII file
 */
class FixFile {

 public:
    FixFile(QIODevice*, const QString&);
    ~FixFile();

    /**
     * Read next line into <str>. Returns false on error or EOF.
     */
    bool nextLine(FixString& str);
    bool exists() { return !_openError; }
    unsigned len() { return _len; }
    unsigned current() { return _current - _base; }
    bool setCurrent(unsigned pos);
    void rewind() { setCurrent(0); }

 private:
    char *_base, *_current;
    QByteArray _data;
    unsigned _len, _currentLeft;
    bool _used_mmap, _openError;
    QIODevice* _file;
    QString _filename;
};


/**
 * A list of pointers, only able to append items.
 * Optimized for speed, not space.
 */
template<class type>
class AppendList {

 public:
  AppendList();
    ~AppendList() { clear(); }

    void setAutoDelete(bool);
    void clear();
    void append(const type*);

    unsigned count() const { return _count; }
    unsigned containsRef(const type*) const;

    type* current();
    type* first();
    type* next();

 private:
    static const int firstLen = 8;
    static const int maxLen = 256;

    struct AppendListChunk {
	int size;
	struct AppendListChunk* next;
	type* data[1];
    };

    struct AppendListChunk *_next, *_current, *_last;
    int _count, _currentIndex, _lastIndex;
    bool _autoDelete;
    type* _first[firstLen];
};


#endif
