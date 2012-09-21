TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

contains(QT_CONFIG, dbus) {
    DEFINES += QT_DBUS_SUPPORT
    QT += dbus
}

include(../libcore/libcore.pri)
include(../libviews/libviews.pri)

# needed for $$QMAKE_MOC below to become defined
load(moc)

# moc handling of code shared with KDE the "KDE way":
# this generates *.moc files from HEADERS, which get included from *.cpp
new_moc.CONFIG = no_link moc_verify
new_moc.output  = ${QMAKE_FILE_BASE}.moc
new_moc.commands = $$moc_header.commands
new_moc.input = NHEADERS
QMAKE_EXTRA_COMPILERS = new_moc

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
