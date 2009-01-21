include_directories(../libcore)
#link_directories(${kcachegrind_BINARY_DIR}/libcore)
add_executable(cgview main.cpp)
target_link_libraries(cgview core ${QT_QTCORE_LIBRARY} ${QT_QT3SUPPORT_LIBRARY})
