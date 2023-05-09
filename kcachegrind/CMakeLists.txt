########### next target ###############

set(kcachegrind_SRCS
   main.cpp
   kdeconfig.cpp
   toplevel.cpp
   configdlg.cpp
   ${kcachegrind_QM_LOADER}
   )

ki18n_wrap_ui(kcachegrind_SRCS configdlgbase.ui dumpselectionbase.ui)

qt_add_resources(kcachegrind_SRCS
    kcachegrind.qrc
)

set(kcachegrind_ICONS
    32-apps-kcachegrind.png
    48-apps-kcachegrind.png
    64-apps-kcachegrind.png
    128-apps-kcachegrind.png
)

ecm_add_app_icon(kcachegrind_SRCS ICONS ${kcachegrind_ICONS})

add_executable(kcachegrind ${kcachegrind_SRCS})

set(kcachegrind_LIBS
  KF${KF_MAJOR_VERSION}::Archive
  KF${KF_MAJOR_VERSION}::CoreAddons
  KF${KF_MAJOR_VERSION}::I18n
  KF${KF_MAJOR_VERSION}::XmlGui
  KF${KF_MAJOR_VERSION}::ConfigCore
  KF${KF_MAJOR_VERSION}::ConfigGui
  KF${KF_MAJOR_VERSION}::KIOCore
  KF${KF_MAJOR_VERSION}::KIOWidgets
  KF${KF_MAJOR_VERSION}::DBusAddons
)

target_link_libraries(kcachegrind core views ${kcachegrind_LIBS})

install(TARGETS kcachegrind  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install( PROGRAMS org.kde.kcachegrind.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.kcachegrind.appdata.xml DESTINATION  ${KDE_INSTALL_METAINFODIR} )
install( FILES tips  DESTINATION  ${KDE_INSTALL_DATADIR}/kcachegrind )

ecm_install_icons(
    ICONS ${kcachegrind_ICONS}
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)


