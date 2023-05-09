set(core_SRCS
   context.cpp
   costitem.cpp
   eventtype.cpp
   subcost.cpp
   addr.cpp
   tracedata.cpp
   loader.cpp
   cachegrindloader.cpp
   fixcost.cpp
   pool.cpp
   coverage.cpp
   stackbrowser.cpp
   utils.cpp
   logger.cpp
   config.cpp
   globalconfig.cpp )

add_library(core STATIC ${core_SRCS})
target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(core
    Qt::Core
)
