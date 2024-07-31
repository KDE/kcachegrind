add_executable(qcachegrind)

qt6_wrap_ui(qcachegrind_UI_SRCS
    generalsettings.ui
    sourcesettings.ui
    colorsettings.ui
)

target_sources(qcachegrind PRIVATE
    ${qcachegrind_UI_SRCS}
    qcgmain.cpp
    qcgtoplevel.cpp
    qcgconfig.cpp
    configdialog.cpp
    configpage.cpp
    qtcolorbutton.cpp
    generalsettings.cpp
    sourcesettings.cpp
    colorsettings.cpp

    qcgtoplevel.h
    qcgconfig.h
    configdialog.h
    configpage.h
    qtcolorbutton.h
    generalsettings.h
    sourcesettings.h
    colorsettings.h

    qcachegrind.qrc
)

target_link_libraries(qcachegrind
    views
    core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
)

# do not install example code...
# install(TARGETS qcachegrind ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
