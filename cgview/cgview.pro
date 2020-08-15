TEMPLATE = app
QT -= gui
CONFIG += console

include(../libcore/libcore.pri)

# This generate *.moc files from NHEADERS, which get included from *.cpp
new_moc.CONFIG = no_link moc_verify
new_moc.output  = ${QMAKE_FILE_BASE}.moc
new_moc.commands = $$moc_header.commands
new_moc.input = NHEADERS
QMAKE_EXTRA_COMPILERS = new_moc

SOURCES += main.cpp

# makes headers visible in qt-creator
HEADERS += $$NHEADERS
