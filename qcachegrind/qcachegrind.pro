TEMPLATE = app

contains(QT_CONFIG, dbus) {
    DEFINES += QT_DBUS_SUPPORT
    QT += dbus
}

OBJECTS_DIR = $$OUT_PWD/../.obj

include(../libcore/libcore.pri)
include(../libviews/libviews.pri)

# Do moc handling of code shared with KDE the "KDE way":
# This generate *.moc files from NHEADERS, which get included from *.cpp
new_moc.CONFIG = no_link moc_verify
new_moc.output  = ${QMAKE_FILE_BASE}.moc
new_moc.commands = moc ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
# moc output only depends on file defining the QObject subclass
new_moc.depend_command = echo ${QMAKE_FILE_NAME}
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
