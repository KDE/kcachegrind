cmake_minimum_required(VERSION 2.6)

project(kcachegrind)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

set( CMAKE_REQUIRED_DEFINITIONS ${_KDE4_PLATFORM_DEFINITIONS} -DQT_STRICT_ITERATORS )
add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR}	${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} )

set( KCACHEGRIND_VERSION "0.7.4kde")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h )

IF(NOT WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.spec.cmake ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec )
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.lsm.cmake ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.lsm )
ENDIF(NOT WIN32)

macro_additional_clean_files( 
	${CMAKE_CURRENT_BINARY_DIR}/version.h 
	)
IF(NOT WIN32)
macro_additional_clean_files(
	${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.lsm 
	${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec
	)
ENDIF(NOT WIN32)

add_subdirectory( doc )
add_subdirectory( libcore )
add_subdirectory( cgview )
add_subdirectory( libviews )
add_subdirectory( kcachegrind )
add_subdirectory( qcachegrind )
add_subdirectory( pics )
add_subdirectory( converters )
