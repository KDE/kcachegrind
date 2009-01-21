#! /bin/sh
(cd kcachegrind;$PREPARETIPS) > tips.cpp
$XGETTEXT rc.cpp `find kcachegrind -name '*.cpp'` tips.cpp -o $podir/kcachegrind.pot
$XGETTEXT_QT `find libcore -name '*.cpp'` -o $podir/kcachegrind_qt.pot
rm -f tips.cpp
