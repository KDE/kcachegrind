include_directories( ../libcore )

set(libviews_SRCS
   globalguiconfig.cpp
   stackitem.cpp
   stackselection.cpp
   partgraph.cpp
   partselection.cpp
   costlistitem.cpp
   functionlistmodel.cpp
   functionselection.cpp
   toplevelbase.cpp
   listutils.cpp
   treemap.cpp
   traceitemview.cpp
   tabview.cpp
   multiview.cpp
   instrview.cpp
   sourceview.cpp
   callmapview.cpp
   callgraphview.cpp
   callview.cpp
   coverageview.cpp
   eventtypeview.cpp
   partview.cpp
   eventtypeitem.cpp
   callitem.cpp
   coverageitem.cpp
   sourceitem.cpp
   instritem.cpp
   partlistitem.cpp )

qt4_automoc(${libviews_SRCS})
add_library(views STATIC ${libviews_SRCS})
target_link_libraries(views core)
