set(qcachegrind_SRCS qcgmain.cpp qcgtoplevel.cpp qcgconfig.cpp
		     configdialog.cpp configpage.cpp qtcolorbutton.cpp
		     generalsettings.cpp sourcesettings.cpp colorsettings.cpp)

qt_wrap_ui(qcachegrind_H generalsettings.ui sourcesettings.ui colorsettings.ui)
qt_add_resources(qcachegrind_RSRC qcachegrind.qrc)

add_executable(qcachegrind ${qcachegrind_SRCS} ${qcachegrind_H} ${qcachegrind_RSRC})

target_link_libraries(qcachegrind
        core views
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::DBus)

# do not install example code...
# install(TARGETS qcachegrind ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
