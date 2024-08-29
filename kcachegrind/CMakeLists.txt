set(kcachegrind_ICONS
    32-apps-kcachegrind.png
    48-apps-kcachegrind.png
    64-apps-kcachegrind.png
    128-apps-kcachegrind.png
)

add_executable(kcachegrind)

target_sources(kcachegrind PRIVATE
   main.cpp
   kdeconfig.cpp
   kdeconfig.h
   toplevel.cpp
   toplevel.h
   configdlg.cpp
   configdlg.h

   kcachegrind.qrc
)

ecm_create_qm_loader(kcachegrind kcachegrind_qt)

ki18n_wrap_ui(kcachegrind
    configdlgbase.ui
    dumpselectionbase.ui
)

ecm_add_app_icon(kcachegrind ICONS ${kcachegrind_ICONS})

target_link_libraries(kcachegrind
    core
    views
    KF6::Archive
    KF6::CoreAddons
    KF6::I18n
    KF6::XmlGui
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::KIOCore
    KF6::KIOWidgets
    KF6::DBusAddons
    KF6::Crash
)

install(TARGETS kcachegrind  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )

install( PROGRAMS org.kde.kcachegrind.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.kcachegrind.appdata.xml DESTINATION  ${KDE_INSTALL_METAINFODIR} )
install( FILES tips  DESTINATION  ${KDE_INSTALL_DATADIR}/kcachegrind )

ecm_install_icons(
    ICONS ${kcachegrind_ICONS}
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)


