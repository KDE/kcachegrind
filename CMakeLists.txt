cmake_minimum_required (VERSION 3.0)

project(kcachegrind)

# minimal requirements
set (QT_MIN_VERSION "5.6.0")
set (KF_MIN_VERSION "5.18.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(FeatureSummary)
include(ECMAddAppIcon)
include(ECMPoQmTools)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED Core DBus Gui Widgets)

find_package(KF5 ${KF_MIN_VERSION} REQUIRED
    Archive
    CoreAddons
    DocTools
    WidgetsAddons
    XmlGui
    I18n
    Config
    KIO
    DBusAddons
)

add_definitions(
    -DQT_DEPRECATED_WARNINGS
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050600
    -DQT_USE_QSTRINGBUILDER
    -DQT_NO_CAST_TO_ASCII
#     -DQT_NO_CAST_FROM_ASCII
#     -DQT_NO_CAST_FROM_BYTEARRAY
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_NO_SIGNALS_SLOTS_KEYWORDS
)

set( KCACHEGRIND_VERSION "0.8.0kde")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h )

IF(NOT WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.spec.cmake ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec )
ENDIF(NOT WIN32)

ecm_create_qm_loader(kcachegrind_QM_LOADER kcachegrind_qt)

add_subdirectory( doc )
add_subdirectory( libcore )
add_subdirectory( cgview )
add_subdirectory( libviews )
add_subdirectory( kcachegrind )
add_subdirectory( qcachegrind )
add_subdirectory( pics )
add_subdirectory( converters )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
