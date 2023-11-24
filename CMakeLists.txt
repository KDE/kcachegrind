cmake_minimum_required (VERSION 3.0)

# KDE Application Version, managed by release script
set(RELEASE_SERVICE_VERSION_MAJOR "24")
set(RELEASE_SERVICE_VERSION_MINOR "01")
set(RELEASE_SERVICE_VERSION_MICRO "80")
set(KCACHEGRIND_VERSION ${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO})

project(kcachegrind VERSION ${KCACHEGRIND_VERSION})

# minimal requirements
set (QT_MIN_VERSION "6.4.0")
set (KF_MIN_VERSION "5.56.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(FeatureSummary)
include(ECMAddAppIcon)
include(ECMPoQmTools)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED Core DBus Gui Widgets)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED
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
    -DQT_USE_QSTRINGBUILDER
    -DQT_NO_CAST_TO_ASCII
#     -DQT_NO_CAST_FROM_ASCII
#     -DQT_NO_CAST_FROM_BYTEARRAY
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_NO_SIGNALS_SLOTS_KEYWORDS
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h )

IF(NOT WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kcachegrind.spec.in ${CMAKE_CURRENT_BINARY_DIR}/kcachegrind.spec )
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

ki18n_install(po)
kdoctools_install(po)
ecm_install_po_files_as_qm(poqm)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
