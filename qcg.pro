# Build file for QCachegrind using qmake
# This does not require KDE in any way (neither at build nor run time)

lessThan(QT_VERSION, 4.4) {
    error("QCachegrind requires Qt 4.4 or greater")
}

TEMPLATE = subdirs
SUBDIRS = cgview qcachegrind
