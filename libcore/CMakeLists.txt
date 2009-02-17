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

qt4_automoc(${core_SRCS})
add_library(core STATIC ${core_SRCS})
