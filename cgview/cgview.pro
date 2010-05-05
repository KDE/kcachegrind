TEMPLATE = app
QT -= gui
TARGET =

OBJECTS_DIR = $$OUT_PWD/../.obj

include(../libcore/libcore.pri)

# Do moc handling the KDE way: we share libcore with KDE code
#
# This generate *.moc files from NHEADERS, which get included from *.cpp
new_moc.CONFIG = no_link moc_verify
new_moc.output  = ${QMAKE_FILE_BASE}.moc
new_moc.commands = moc ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
new_moc.depend_command = g++ -E -M ${QMAKE_FILE_NAME} | sed "s,^.*: ,,"
new_moc.input = NHEADERS
QMAKE_EXTRA_COMPILERS = new_moc

SOURCES += main.cpp
