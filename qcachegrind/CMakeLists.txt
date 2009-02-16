include_directories(../libcore ../libviews)

set(qcachegrind_SRCS main.cpp toplevel.cpp qtconfig.cpp)

qt4_automoc(${qcachegrind_SRCS})
qt4_add_resources(qcachegrind_RSRC qcachegrind.qrc)

add_executable(qcachegrind ${qcachegrind_SRCS} ${qcachegrind_RSRC})

target_link_libraries(qcachegrind
	 core views
	 ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY}
	  ${QT_QTDBUS_LIBRARY} ${QT_QT3SUPPORT_LIBRARY})

# do not install example code...
# install(TARGETS qcachegrind ${INSTALL_TARGETS_DEFAULT_ARGS} )
