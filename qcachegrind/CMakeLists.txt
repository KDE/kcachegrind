include_directories(../libcore ../libviews)

set(qcachegrind_SRCS main.cpp toplevel.cpp)

qt4_automoc(${qcachegrind_SRCS})

add_executable(qcachegrind ${qcachegrind_SRCS})

target_link_libraries(qcachegrind
	 core views
	 ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY}
	  ${QT_QTDBUS_LIBRARY} ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS qcachegrind ${INSTALL_TARGETS_DEFAULT_ARGS} )
