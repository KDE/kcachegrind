TEMPLATE = app

include (../version.pri)
mac {
    QMAKE_TARGET_BUNDLE_PREFIX = io.github.kcachegrind
    # For the bundle name etc.
    TARGET = QCachegrind
    QMAKE_INFO_PLIST = qcachegrind.plist
}
QMAKE_TARGET_PRODUCT = QCacheGrind
QMAKE_TARGET_DESCRIPTION = QCacheGrind

# Assembled from the KCacheGrind icon PNGs
RC_ICONS = ../logo.ico
# Filename is significant on macOS
ICON = ../logo.icns

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

greaterThan(QT_VERSION, 5.7) {
    CONFIG += c++11
}

contains(QT_CONFIG, dbus) {
    DEFINES += QT_DBUS_SUPPORT
    QT += dbus
}

include(../libcore/libcore.pri)
include(../libviews/libviews.pri)

# Input
FORMS += generalsettings.ui sourcesettings.ui colorsettings.ui

RESOURCES = qcachegrind.qrc

NHEADERS += \
    qcgconfig.h \
    qcgtoplevel.h \
    configdialog.h \
    configpage.h \
    generalsettings.h \
    sourcesettings.h \
    qtcolorbutton.h \
    colorsettings.h

SOURCES += qcgmain.cpp \
    qcgconfig.cpp \
    qcgtoplevel.cpp \
    configdialog.cpp \
    configpage.cpp \
    generalsettings.cpp \
    sourcesettings.cpp \
    qtcolorbutton.cpp \
    colorsettings.cpp

# makes headers visible in qt-creator (luckily no double moc handling)
HEADERS += $$NHEADERS
