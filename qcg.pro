# Build file for QCachegrind using qmake
# This does not require KDE in any way (neither at build nor run time)


lessThan(QT_MAJOR_VERSION, 5) {
    error("QCachegrind requires Qt 5.3 or greater")
}

lessThan(QT_MINOR_VERSION, 3) {
    error("QCachegrind requires Qt 5.3 or greater")
}


TEMPLATE = subdirs
SUBDIRS = cgview qcachegrind
