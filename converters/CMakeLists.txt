configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/hotshot2calltree.in
	${CMAKE_CURRENT_BINARY_DIR}/hotshot2calltree
	)

install( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/hotshot2calltree
	 op2calltree pprof2calltree dprof2calltree memprof2calltree
	 DESTINATION ${KDE_INSTALL_BINDIR} )
