include_directories( ../libcore )

########### next target ###############

set(kcachegrind_SRCS 
   treemap.cpp 
   main.cpp 
   configuration.cpp 
   functionselection.cpp 
   coverage.cpp 
   partgraph.cpp 
   toplevel.cpp 
   stackselection.cpp 
   stackbrowser.cpp 
   partselection.cpp 
   configdlg.cpp 
   traceitemview.cpp 
   instrview.cpp 
   tabview.cpp 
   sourceview.cpp 
   callmapview.cpp 
   callview.cpp 
   coverageview.cpp 
   eventtypeview.cpp 
   partview.cpp 
   listutils.cpp 
   eventtypeitem.cpp 
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

kde4_add_ui_files(kcachegrind_SRCS
	stackselectionbase.ui
	functionselectionbase.ui
	partselectionbase.ui
        configdlgbase.ui 
	)

kde4_add_app_icon(kcachegrind_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/hi*-app-kcachegrind.png")

kde4_add_executable(kcachegrind ${kcachegrind_SRCS})

target_link_libraries(kcachegrind core ${KDE4_KIO_LIBS} ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS kcachegrind  ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install( FILES kcachegrind.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES tips  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )
install( FILES kcachegrindui.rc  DESTINATION  ${DATA_INSTALL_DIR}/kcachegrind )

kde4_install_icons( ${ICON_INSTALL_DIR}   )


