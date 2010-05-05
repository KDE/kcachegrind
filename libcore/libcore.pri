INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

NHEADERS += \
    context.h \
    costitem.h \
    subcost.h \
    eventtype.h \
    addr.h \
    config.h \
    globalconfig.h \
    tracedata.h \
    utils.h \
    logger.h \
    loader.h \
    fixcost.h \
    pool.h \
    coverage.h \
    stackbrowser.h

SOURCES += \
    context.cpp \
    costitem.cpp \
    subcost.cpp \
    eventtype.cpp \
    addr.cpp \
    cachegrindloader.cpp \
    config.cpp \
    coverage.cpp \
    fixcost.cpp \
    globalconfig.cpp \
    loader.cpp \
    logger.cpp \
    pool.cpp \
    stackbrowser.cpp \
    tracedata.cpp \
    utils.cpp
