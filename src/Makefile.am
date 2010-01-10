NULL=

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/pcmanfm2"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/pcmanfm2/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = pcmanfm2

pcmanfm2_SOURCES = \
	pcmanfm.c \
	app-config.c app-config.h \
	main-win.c main-win.h \
	desktop.c desktop.h \
	pref.c pref.h \
	$(NULL)

pcmanfm2_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(FM_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Werror-implicit-function-declaration \
	$(NULL)

pcmanfm2_LDADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GMODULE_LIBS) \
	$(FM_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(NULL)
