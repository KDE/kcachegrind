include_directories( ../libcore ../libviews )

########### next target ###############

set(kcachegrind_SRCS
   main.cpp
   kdeconfig.cpp
   toplevel.cpp
   configdlg.cpp
   )

ki18n_wrap_ui(kcachegrind_SRCS configdlgbase.ui)

kde4_add_app_icon(kcachegrind_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/hi*-apps-kcachegrind.png")

add_executable(kcachegrind ${kcachegrind_SRCS})

set(kcachegrind_LIBS
                 KF5::KDELibs4Support
                 KF5::Archive
)

target_link_libraries(kcachegrind core views ${kcachegrind_LIBS})

install(TARGETS kcachegrind  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install( PROGRAMS org.kde.kcachegrind.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES tips  DESTINATION  ${KDE_INSTALL_DATADIR}/kcachegrind )
install( FILES kcachegrindui.rc  DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kcachegrind )

ecm_install_icons( ICONS hi32-apps-kcachegrind.png  hi48-apps-kcachegrind.png DESTINATION ${KDE_INSTALL_ICONDIR}   )


