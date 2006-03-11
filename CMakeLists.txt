kde4_header()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h )

add_subdirectory( kcachegrind ) 
add_subdirectory( pics ) 
add_subdirectory( converters ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#SUBDIRS = kcachegrind pics converters
#
#EXTRA_DIST = \
#	AUTHORS COPYING NEWS ChangeLog INSTALL README TODO \
#	kcachegrind.lsm kcachegrind.spec version.h 
#
