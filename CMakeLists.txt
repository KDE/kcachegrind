
project(kcachegrind)

set( KCACHEGRIND_VERSION "0.5.0kde")

include(CheckFunctionExists)
check_function_exists(mmap            HAVE_MMAP)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kcachegrind.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kcachegrind.h )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h )

IF(NOT WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.spec.cmake ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec )
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.lsm.cmake ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.lsm )
ENDIF(NOT WIN32)

macro_additional_clean_files( 
	${CMAKE_CURRENT_BINARY_DIR}/version.h 
	${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.lsm 
	${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec)


add_subdirectory( kcachegrind ) 
add_subdirectory( pics ) 
add_subdirectory( converters ) 
