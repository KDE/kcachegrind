
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/.. ) 



########### next target ###############

set(kcachegrind_SRCS 
   loader.cpp 
   cachegrindloader.cpp 
   treemap.cpp 
   pool.cpp 
   main.cpp 
   configuration.cpp 
   functionselection.cpp 
   coverage.cpp 
   partgraph.cpp 
   toplevel.cpp 
   stackselection.cpp 
   stackbrowser.cpp 
   subcost.cpp 
   tracedata.cpp 
   partselection.cpp 
   configdlg.cpp 
   utils.cpp 
   fixcost.cpp 
   traceitemview.cpp 
   instrview.cpp 
   tabview.cpp 
   sourceview.cpp 
   callmapview.cpp 
   callview.cpp 
   coverageview.cpp 
   costtypeview.cpp 
   partview.cpp 
   listutils.cpp 
   costtypeitem.cpp 
   multiview.cpp 
   callitem.cpp 
   coverageitem.cpp 
   sourceitem.cpp 
   costlistitem.cpp 
   partlistitem.cpp 
   functionitem.cpp 
   instritem.cpp 
   stackitem.cpp 
   callgraphview.cpp )

kde4_automoc(${kcachegrind_SRCS})

kde4_add_ui3_files(kcachegrind_SRCS 
   functionselectionbase.ui 
   stackselectionbase.ui 
   partselectionbase.ui 
   configdlgbase.ui )

kde4_add_executable(kcachegrind ${kcachegrind_SRCS})

target_link_libraries(kcachegrind  ${KDE4_KIO_LIBS} )

install(TARGETS kcachegrind  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES kcachegrind.desktop  DESTINATION  ${XDG_APPS_DIR} )
install( FILES tips  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )
install( FILES kcachegrindui.rc  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )
install( FILES x-kcachegrind.desktop  DESTINATION  ${MIME_INSTALL_DIR}/application )

kde4_install_icons( ${ICON_INSTALL_DIR}  hicolor )
kde4_install_icons( ${ICON_INSTALL_DIR}  locolor )


