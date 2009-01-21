# FIXME: Only for compiling now
include_directories(
	../kcachegrind )

kde4_add_library(core STATIC
   cachegrindloader.cpp
   fixcost.cpp
   loader.cpp
   pool.cpp
   subcost.cpp
   tracedata.cpp
   utils.cpp )

