#
# Build file for QCachegrind, using qmake
#
# Simple way to build QCachegrind without KDE libs;
# CMake runs require KDE as this always builds kcachegrind.
#
# To keep NHEADERS/SOURCES in sync on lib changes, replace below
# definitions with the ones in tmp.pro generated from a run with
#  qmake -project -o tmp.pro ../libcore/*.{h,cpp} ../libviews/*.{h,cpp}
#

TEMPLATE = app
QT += qt3support
unix:QT += dbus
TARGET =
DEPENDPATH += . ../libcore ../libviews
INCLUDEPATH += . ../libcore ../libviews
RESOURCES = qcachegrind.qrc

# generate *.moc files from headers, they get included from *.cpp
new_moc.CONFIG = no_link moc_verify
new_moc.output  = ${QMAKE_FILE_BASE}.moc
new_moc.commands = moc ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
new_moc.depend_command = g++ -E -M ${QMAKE_FILE_NAME} | sed "s,^.*: ,,"
new_moc.input = NHEADERS
QMAKE_EXTRA_COMPILERS = new_moc

# Input
FORMS += generalsettings.ui

NHEADERS += qcgconfig.h \
           qcgtoplevel.h \
           configdialog.h \
           configpage.h \
           generalsettings.h \
           ../libcore/context.h \
           ../libcore/costitem.h \
           ../libcore/subcost.h \
           ../libcore/eventtype.h \
           ../libcore/addr.h \
           ../libcore/config.h \
           ../libcore/globalconfig.h \
           ../libcore/tracedata.h \
           ../libcore/utils.h \
           ../libcore/logger.h \
           ../libcore/loader.h \
           ../libcore/fixcost.h \
           ../libcore/pool.h \
           ../libcore/coverage.h \
           ../libcore/stackbrowser.h \
           ../libviews/traceitemview.h \
           ../libviews/toplevelbase.h \
           ../libviews/partselection.h \
           ../libviews/functionselection.h \
           ../libviews/listutils.h \
           ../libviews/stackselection.h \
           ../libviews/multiview.h \
           ../libviews/tabview.h \
           ../libviews/callgraphview.h \
           ../libviews/treemap.h \
           ../libviews/callitem.h \
           ../libviews/callview.h \
           ../libviews/callmapview.h \
           ../libviews/costlistitem.h \
           ../libviews/coverageitem.h \
           ../libviews/coverageview.h \
           ../libviews/eventtypeitem.h \
           ../libviews/eventtypeview.h \
           ../libviews/functionitem.h \
           ../libviews/instritem.h \
           ../libviews/instrview.h \
           ../libviews/partgraph.h \
           ../libviews/partlistitem.h \
           ../libviews/partview.h \
           ../libviews/sourceitem.h \
           ../libviews/sourceview.h \
           ../libviews/stackitem.h

SOURCES += qcgmain.cpp \
           qcgconfig.cpp \
           qcgtoplevel.cpp \
           configdialog.cpp \
           configpage.cpp \
           generalsettings.cpp \
           ../libcore/context.cpp \
           ../libcore/costitem.cpp \
           ../libcore/subcost.cpp \
           ../libcore/eventtype.cpp \
           ../libcore/addr.cpp \
           ../libcore/cachegrindloader.cpp \
           ../libcore/config.cpp \
           ../libcore/coverage.cpp \
           ../libcore/fixcost.cpp \
           ../libcore/globalconfig.cpp \
           ../libcore/loader.cpp \
           ../libcore/logger.cpp \
           ../libcore/pool.cpp \
           ../libcore/stackbrowser.cpp \
           ../libcore/tracedata.cpp \
           ../libcore/utils.cpp \
           ../libviews/callgraphview.cpp \
           ../libviews/callitem.cpp \
           ../libviews/callmapview.cpp \
           ../libviews/callview.cpp \
           ../libviews/costlistitem.cpp \
           ../libviews/coverageitem.cpp \
           ../libviews/coverageview.cpp \
           ../libviews/eventtypeitem.cpp \
           ../libviews/eventtypeview.cpp \
           ../libviews/functionitem.cpp \
           ../libviews/functionselection.cpp \
           ../libviews/instritem.cpp \
           ../libviews/instrview.cpp \
           ../libviews/listutils.cpp \
           ../libviews/multiview.cpp \
           ../libviews/partgraph.cpp \
           ../libviews/partlistitem.cpp \
           ../libviews/partselection.cpp \
           ../libviews/partview.cpp \
           ../libviews/sourceitem.cpp \
           ../libviews/sourceview.cpp \
           ../libviews/stackitem.cpp \
           ../libviews/stackselection.cpp \
           ../libviews/tabview.cpp \
           ../libviews/toplevelbase.cpp \
           ../libviews/traceitemview.cpp \
           ../libviews/treemap.cpp
