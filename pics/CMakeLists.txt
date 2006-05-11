kde4_header()

add_subdirectory( hicolor ) 




########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kcachegrind/icons FILES  )

kde4_footer()



#original Makefile.am contents follow:

#kcachegrindicondir = $(kde_datadir)/kcachegrind/icons
#kcachegrindicon_ICON = AUTO
#SUBDIRS = hicolor
