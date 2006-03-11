Summary:   KDE Profiling Visualisation Tool
Name:      kcachegrind
Version:   ${KCACHEGRIND_VERSION}
Release:   1
Copyright: GPL
Group:     Development/Tools
Vendor:    (none)
URL:       http://kcachegrind.sourceforge.net
Packager:  Josef Weidendorfer <Josef.Weidendorfer@gmx.de>
Source:    kcachegrind-${KCACHEGRIND_VERSION}.tar.gz
BuildRoot: /var/tmp/build

%description
KCachegrind is a GPL'd tool for quick browsing in and visualisation
of performance data of an application run. This data is produced by
profiling tools and typically includes distribution of cost events
to source code ranges (instructions, source lines, functions, C++ classes)
and call relationship of functions.
KCachegrind has a list of functions sorted according to different cost
types, and can provide various performance views for a function like
direct/indirect callers/callees, TreeMap visualisation of cost distribution
among callees, call graph sectors centered around the function and
annotated source/assembler.
Currently, KCachegrind depends on data delivered by the profiling tool
calltree, powered by the Valgrind runtime instrumentation framework.

%prep
%setup
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" ./configure \
                 \
                $LOCALFLAGS
%build
# Setup for parallel builds
numprocs=`egrep -c ^cpu[0-9]+ /proc/stat || :`
if [ "$numprocs" = "0" ]; then
  numprocs=1
fi

make -j$numprocs

%install
make install-strip DESTDIR=$RPM_BUILD_ROOT

cd $RPM_BUILD_ROOT
find . -type d | sed '1,2d;s,^\.,\%attr(-\,root\,root) \%dir ,' > $RPM_BUILD_DIR/file.list.kcachegrind
find . -type f | sed 's,^\.,\%attr(-\,root\,root) ,' >> $RPM_BUILD_DIR/file.list.kcachegrind
find . -type l | sed 's,^\.,\%attr(-\,root\,root) ,' >> $RPM_BUILD_DIR/file.list.kcachegrind

%clean
rm -rf $RPM_BUILD_ROOT/*
rm -rf $RPM_BUILD_DIR/kcachegrind
rm -rf ../file.list.kcachegrind


%files -f ../file.list.kcachegrind
