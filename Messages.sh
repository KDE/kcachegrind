#! /bin/sh
(cd kcachegrind;$PREPARETIPS) > tips.cpp
$EXTRACTRC `find kcachegrind -name \*.ui -o -name \*.rc -o -name \*.kcfg` >> rc.cpp
$XGETTEXT rc.cpp `find kcachegrind -name '*.cpp'` tips.cpp -o $podir/kcachegrind.pot
$EXTRACT_TR_STRINGS `find libcore -name '*.cpp'` `find libviews -name '*.cpp'` -o $podir/kcachegrind_qt.pot
rm -f tips.cpp rc.cpp
