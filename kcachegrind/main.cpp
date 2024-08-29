/*
    This file is part of KCachegrind.

    SPDX-FileCopyrightText: 2003-2016 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only
*/

/*
 * KCachegrind startup
 */

// for KCACHEGRIND_VERSION
#include "../version.h"


#include <QApplication>
#include <QCommandLineParser>

#include <KAboutData>
#include <KCrash>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KDBusService>

#include "kdeconfig.h"
#include "toplevel.h"
#include "tracedata.h"
#include "loader.h"

int main( int argc, char ** argv )
{
    QApplication a(argc, argv);
    KLocalizedString::setApplicationDomain("kcachegrind");
    KAboutData aboutData(QStringLiteral("kcachegrind"),
                         i18n("KCachegrind"),
                         KCACHEGRIND_VERSION,
                         i18n("KDE Frontend for Callgrind/Cachegrind"),
                         KAboutLicense::GPL,
                         i18n("(C) 2002 - 2016"), QString(),
                         QStringLiteral("https://kcachegrind.github.io"));
    aboutData.addAuthor(i18n("Josef Weidendorfer"),
                        i18n("Author/Maintainer"),
                        QStringLiteral("Josef.Weidendorfer@gmx.de"));
    aboutData.setOrganizationDomain("kde.org");
    aboutData.setDesktopFileName(QStringLiteral("org.kde.kcachegrind"));

    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kcachegrind")));

    KCrash::initialize();

    KDBusService service(KDBusService::Multiple);

    //   KGlobal::locale()->insertCatalog("kcachegrind_qt");
    Loader::initLoaders();

    KConfig* kc = KSharedConfig::openConfig().data();
    ConfigStorage::setStorage(new KDEConfigStorage(kc));

    if (a.isSessionRestored()){
        int n = 1;
        while (KMainWindow::canBeRestored(n)){
            (new TopLevel())->restore(n);
            n++;
        }
    }
    else {
        TopLevel* t;

        QCommandLineParser parser;
        parser.addPositionalArgument(QStringLiteral("trace"), i18n("Show information of this trace"), i18n("[trace...]"));
        aboutData.setupCommandLine(&parser);
        parser.process(a);
        aboutData.processCommandLine(&parser);

        int nbArgs = parser.positionalArguments().count();
        if (nbArgs>0) {
            t = new TopLevel();
            t->show();
            foreach(const QString &arg, parser.positionalArguments()) {
                t->loadDelayed(arg);
            }
        }
        else {
            // load trace in current dir
            t = new TopLevel();
            t->show();
            t->loadDelayed(QStringLiteral("."));
        }
    }

    a.connect( &a, &QGuiApplication::lastWindowClosed, &a, &QCoreApplication::quit );
    int res = a.exec();

    // to make leak checking in valgrind happy...
    Loader::deleteLoaders();
    ProfileContext::cleanup();
    ConfigStorage::cleanup();

    return res;
}
