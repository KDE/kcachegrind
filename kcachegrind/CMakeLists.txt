include_directories( ../libcore ../libviews )

########### next target ###############

set(kcachegrind_SRCS
   main.cpp
   kdeconfig.cpp
   toplevel.cpp
   configdlg.cpp
   )

qt5_wrap_ui(kcachegrind_SRCS configdlgbase.ui)

kde4_add_app_icon(kcachegrind_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/hi*-app-kcachegrind.png")

add_executable(kcachegrind ${kcachegrind_SRCS})

set(kcachegrind_LIBS
                 KF5::KDELibs4Support
                 KF5::Archive
)

target_link_libraries(kcachegrind core views ${kcachegrind_LIBS})

install(TARGETS kcachegrind  ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install( PROGRAMS kcachegrind.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES tips  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )
install( FILES kcachegrindui.rc  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )

ecm_install_icons( ${ICON_INSTALL_DIR}   )


