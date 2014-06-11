include_directories(../libcore ../libviews)

set(qcachegrind_SRCS qcgmain.cpp qcgtoplevel.cpp qcgconfig.cpp
		     configdialog.cpp configpage.cpp qtcolorbutton.cpp
		     generalsettings.cpp sourcesettings.cpp colorsettings.cpp)

qt5_wrap_ui(qcachegrind_H generalsettings.ui sourcesettings.ui colorsettings.ui)
qt5_add_resources(qcachegrind_RSRC qcachegrind.qrc)

add_executable(qcachegrind ${qcachegrind_SRCS} ${qcachegrind_H} ${qcachegrind_RSRC})

target_link_libraries(qcachegrind
        core views
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::DBus)

# do not install example code...
# install(TARGETS qcachegrind ${INSTALL_TARGETS_DEFAULT_ARGS} )
