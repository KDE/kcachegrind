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

#include "utils.h"

#include <errno.h>

#include <QIODevice>
#include <QFile>



// class FixString

FixString::FixString(const char* str, int len)
{
    _str = str;
    _len = len;
}

bool FixString::stripFirst(char& c)
{
    if (!_len) {
	c = 0;
	return false;
    }

    c = *_str;
    _str++;
    _len--;
    return true;
 }

bool FixString::stripPrefix(const char* p)
{
    if (_len == 0) return false;
    if (!p || (*p != *_str)) return false;

    const char* s = _str+1;
    int l = _len-1;
    p++;
    while(*p) {
	if (l==0) return false;
	if (*s != *p) return false;
	p++;
	s++;
	l--;
    }
    _str = s;
    _len = l;
    return true;
}


// this parses hexadecimal (with prefix '0x' too)
bool FixString::stripUInt(unsigned int& v, bool stripSpaces)
{
    if (_len==0) {
	v = 0;
	return false;
    }

    char c = *_str;
    if (c<'0' || c>'9') {
	v = 0;
	return false;
    }

    v = c-'0';
    const char* s = _str+1;
    int l = _len-1;
    c = *s;

    if ((l>0) && (c == 'x') && (v==0)) {
	// hexadecimal
	s++;
	c = *s;
	l--;

	while(l>0) {
	    if (c>='0' && c<='9')
		v = 16*v + (c-'0');
	    else if (c>='a' && c<='f')
		v = 16*v + 10 + (c-'a');
	    else if (c>='A' && c<='F')
		v = 16*v + 10 + (c-'A');
	    else
		break;
	    s++;
	    c = *s;
	    l--;
	}
    }
    else {
	// decimal

	while(l>0) {
	    if (c<'0' || c>'9') break;
	    v = 10*v + (c-'0');
	    s++;
	    c = *s;
	    l--;
	}
    }

    if (stripSpaces)
      while(l>0) {
        if (c != ' ') break;
        s++;
        c = *s;
        l--;
      }

    _str = s;
    _len = l;
    return true;
}


void FixString::stripSurroundingSpaces()
{
    if (_len==0) return;

    // leading spaces
    while((_len>0) && (*_str==' ')) {
	_len--;
	_str++;
    }

    // trailing spaces
    while((_len>0) && (_str[_len-1]==' ')) {
	_len--;
    }
}

void FixString::stripSpaces()
{
    while((_len>0) && (*_str==' ')) {
	_len--;
	_str++;
    }
}

bool FixString::stripName(FixString& s)
{
    if (_len==0) return false;

    // first char has to be a letter or "_"
    if (!QChar(*_str).isLetter() && (*_str != '_')) return false;

    int newLen = 1;
    const char* newStr = _str;

    _str++;
    _len--;

    while(_len>0) {
      if (!QChar(*_str).isLetterOrNumber()
	  && (*_str != '_')) break;

      newLen++;
      _str++;
      _len--;
    }

    s.set(newStr, newLen);
    return true;
}

FixString FixString::stripUntil(char c)
{
  if (_len == 0) return FixString();

  const char* newStr = _str;
  int newLen = 0;

  while(_len>0) {
    if (*_str == c) {
      _str++;
      _len--;
      break;
    }

    _str++;
    _len--;
    newLen++;
  }
  return FixString(newStr, newLen);
}

bool FixString::stripUInt64(uint64& v, bool stripSpaces)
{
    if (_len==0) {
	v = 0;
	return false;
    }

    char c = *_str;
    if (c<'0' || c>'9') {
	v = 0;
	return false;
    }

    v = c-'0';
    const char* s = _str+1;
    int l = _len-1;
    c = *s;

    if ((l>0) && (c == 'x') && (v==0)) {
	// hexadecimal
	s++;
	c = *s;
	l--;

	while(l>0) {
	    if (c>='0' && c<='9')
		v = 16*v + (c-'0');
	    else if (c>='a' && c<='f')
		v = 16*v + 10 + (c-'a');
	    else if (c>='A' && c<='F')
		v = 16*v + 10 + (c-'A');
	    else
		break;
	    s++;
	    c = *s;
	    l--;
	}
    }
    else {
      // decimal
      while(l>0) {
	if (c<'0' || c>'9') break;
	v = 10*v + (c-'0');
	s++;
	c = *s;
	l--;
      }
    }

    if (stripSpaces)
      while(l>0) {
        if (c != ' ') break;
        s++;
        c = *s;
        l--;
      }

    _str = s;
    _len = l;
    return true;
}


bool FixString::stripInt64(int64& v, bool stripSpaces)
{
    if (_len==0) {
	v = 0;
	return false;
    }

    char c = *_str;
    if (c<'0' || c>'9') {
	v = 0;
	return false;
    }

    v = c-'0';
    const char* s = _str+1;
    int l = _len-1;
    c = *s;

    if ((l>0) && (c == 'x') && (v==0)) {
	// hexadecimal
	s++;
	c = *s;
	l--;

	while(l>0) {
	    if (c>='0' && c<='9')
		v = 16*v + (c-'0');
	    else if (c>='a' && c<='f')
		v = 16*v + 10 + (c-'a');
	    else if (c>='A' && c<='F')
		v = 16*v + 10 + (c-'A');
	    else
		break;
	    s++;
	    c = *s;
	    l--;
	}
    }
    else {
      // decimal

      while(l>0) {
	if (c<'0' || c>'9') break;
	v = 10*v + (c-'0');
	s++;
	c = *s;
	l--;
      }
    }

    if (stripSpaces)
      while(l>0) {
        if (c != ' ') break;
        s++;
        c = *s;
        l--;
      }

    _str = s;
    _len = l;
    return true;
}



// class FixFile

FixFile::FixFile(QIODevice* file, const QString& filename)
{
  _file = file;

  if (!file) {
    _len = 0;
    _currentLeft = 0;
    _openError = true;
    return;
  }

  _filename = filename;
  if (!file->isOpen() && !file->open( QIODevice::ReadOnly ) ) {
    qWarning( "%s: %s", (const char*)QFile::encodeName(_filename),
	      strerror( errno ) );
    _len = 0;
    _currentLeft = 0;
    _openError = true;
    return;
  }

  _openError = false;
  _used_mmap = false;

  uchar* addr = 0;

#if QT_VERSION >= 0x040400
  // QFile::map was introduced with Qt 4.4
  if (file->size() >0) {
      QFile* mappableDevice = dynamic_cast<QFile*>(file);
      if (mappableDevice) {
        addr = mappableDevice->map( 0, file->size() );
      }
  }
#endif

  if (addr) {
      // map succeeded
      _base = (char*) addr;
      _len = file->size();
      _used_mmap = true;

      if (0) qDebug("Mapped '%s'", qPrintable( _filename ));
  }
  else {
      // try reading the data into memory instead
      file->seek(0);
      _data = file->readAll();
      _base = _data.data();
      _len  = _data.size();
  }

  _current     = _base;
  _currentLeft = _len;
}

FixFile::~FixFile()
{
    // if the file was read into _data, it will be deleted automatically

    if (_used_mmap && _file) {
	if (0) qDebug("Unmapping '%s'", qPrintable( _filename ));
#if QT_VERSION >= 0x040400
	QFile* mappableDevice = dynamic_cast<QFile*>(_file);
	Q_ASSERT(mappableDevice);
	if (!mappableDevice->unmap( (uchar*) _base ))
	    qWarning( "munmap: %s", strerror( errno ) );
#endif
    }
}

bool FixFile::nextLine(FixString& str)
{
    if (_currentLeft == 0) return false;

    unsigned left = _currentLeft;
    char* current = _current;

    while(left>0) {
	if (*current == 0 || *current == '\n') break;
	current++;
	left--;
    }

    if (0) {
	char tmp[200];
	int l = _currentLeft-left;
	if (l>199) l = 199;
	strncpy(tmp, _current, l);
	tmp[l] = 0;
	qDebug("[FixFile::nextLine] At %lu, len %u: '%s'",
	       (unsigned long) (_current - _base), _currentLeft-left, tmp);
    }

    int len =  _currentLeft-left;
    // get rid of any carriage return at end
    if ((len>0) && (*(current-1) == '\r')) len--;
    str.set(_current, len);

    if (*current == '\n') {
	current++;
	left--;
    }
    _current = current;
    _currentLeft = left;

    return true;
}

bool FixFile::setCurrent(unsigned pos)
{
    if (pos > _len) return false;

    _current = _base + pos;
    _currentLeft = _len - pos;
    return true;
}


#if 0

// class AppendList


AppendList::AppendList()
{
  _next = 0;
  _current = 0;
  _last = 0;

  _count = 0;
  _currentIndex = 0;
  _lastIndex = 0;
  _autoDelete = false;
}


void AppendList::clear()
{
  int count = _count;
  int i;

  if (count <= firstLen) {
    if (_autoDelete)
      for (i=0;i<count;i++)
        delete _first[i];
  }
}

#endif

