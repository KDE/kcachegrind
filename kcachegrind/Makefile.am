bin_PROGRAMS = kcachegrind

kcachegrind_SOURCES = \
 functionselectionbase.ui \
 stackselectionbase.ui \
 partselectionbase.ui \
 configdlgbase.ui \
 loader.cpp cachegrindloader.cpp treemap.cpp pool.cpp \
 main.cpp configuration.cpp \
 functionselection.cpp coverage.cpp partgraph.cpp \
 toplevel.cpp stackselection.cpp stackbrowser.cpp \
 subcost.cpp tracedata.cpp partselection.cpp configdlg.cpp \
 utils.cpp fixcost.cpp \
 traceitemview.cpp instrview.cpp tabview.cpp \
 sourceview.cpp callmapview.cpp callview.cpp \
 coverageview.cpp costtypeview.cpp partview.cpp \
 listutils.cpp costtypeitem.cpp multiview.cpp \
 callitem.cpp coverageitem.cpp sourceitem.cpp \
 costlistitem.cpp partlistitem.cpp functionitem.cpp \
 instritem.cpp stackitem.cpp callgraphview.cpp

kcachegrind_COMPILE_FIRST = ../version.h

kcachegrind_LDADD   =  $(LIB_KIO)

KDE_ICON = AUTO

xdg_apps_DATA = kcachegrind.desktop

mimeapplicationdir = $(kde_mimedir)/application
mimeapplication_DATA = x-kcachegrind.desktop

EXTRA_DIST =              \
 kcachegrind.desktop      \
 x-kcachegrind.desktop    \
 hi32-app-kcachegrind.png \
 hi48-app-kcachegrind.png \
 Doxyfile                 \
 kcachegrindui.rc

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

METASOURCES = AUTO

# the library search path. 
kcachegrind_LDFLAGS = $(all_libraries) $(KDE_RPATH)

rcdir = $(kde_datadir)/kcachegrind
rc_DATA = kcachegrindui.rc

tipdir = $(kde_datadir)/kcachegrind
tip_DATA = tips

messages: rc.cpp
	$(PREPARETIPS) > tips.cpp
	LIST=`find . -name \*.h -o -name \*.cpp`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kcachegrind.pot; \
	fi
	rm -f tips.cpp

