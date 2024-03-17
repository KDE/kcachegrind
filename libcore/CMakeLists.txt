add_library(core STATIC)

target_sources(core PRIVATE
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
   globalconfig.cpp
)

target_link_libraries(core
    Qt6::Core
)
