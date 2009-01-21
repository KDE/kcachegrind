set(core_SRCS
   cachegrindloader.cpp
   fixcost.cpp
   loader.cpp
   pool.cpp
   subcost.cpp
   tracedata.cpp
   coverage.cpp
   stackbrowser.cpp
   utils.cpp
   logger.cpp
   config.cpp
   globalconfig.cpp )

qt4_automoc(${core_SRCS})
add_library(core STATIC ${core_SRCS})
