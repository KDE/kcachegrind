add_executable(cgview main.cpp)

target_link_libraries(cgview
    core
    Qt6::Core
)

# do not install example code...
# install(TARGETS cgview ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
