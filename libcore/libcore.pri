INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

NHEADERS += \
    $$PWD/context.h \
    $$PWD/costitem.h \
    $$PWD/subcost.h \
    $$PWD/eventtype.h \
    $$PWD/addr.h \
    $$PWD/config.h \
    $$PWD/globalconfig.h \
    $$PWD/tracedata.h \
    $$PWD/utils.h \
    $$PWD/logger.h \
    $$PWD/loader.h \
    $$PWD/fixcost.h \
    $$PWD/pool.h \
    $$PWD/coverage.h \
    $$PWD/stackbrowser.h

SOURCES += \
    $$PWD/context.cpp \
    $$PWD/costitem.cpp \
    $$PWD/subcost.cpp \
    $$PWD/eventtype.cpp \
    $$PWD/addr.cpp \
    $$PWD/cachegrindloader.cpp \
    $$PWD/config.cpp \
    $$PWD/coverage.cpp \
    $$PWD/fixcost.cpp \
    $$PWD/globalconfig.cpp \
    $$PWD/loader.cpp \
    $$PWD/logger.cpp \
    $$PWD/pool.cpp \
    $$PWD/stackbrowser.cpp \
    $$PWD/tracedata.cpp \
    $$PWD/utils.cpp
