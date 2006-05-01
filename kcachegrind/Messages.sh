#! /bin/sh
$PREPARETIPS > tips.cpp
LIST=`find . -name \*.h -o -name \*.cpp`
if test -n "$LIST"; then
$XGETTEXT $LIST rc.cpp -o $podir/kcachegrind.pot
fi
rm -f tips.cpp
