NULL=

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/pcmanfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/pcmanfm/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = pcmanfm

pcmanfm_SOURCES = \
	pcmanfm.c pcmanfm.h \
	app-config.c app-config.h \
	main-win.c main-win.h \
	desktop.c desktop.h \
	volume-manager.c volume-manager.h \
	pref.c pref.h \
	utils.c utils.h \
	gseal-gtk-compat.h \
	$(NULL)

EXTRA_DIST= \
	main-win-ui.c \
	desktop-ui.c \
	$(NULL)

pcmanfm_CFLAGS = \
	$(XLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(FM_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall \
	-Werror-implicit-function-declaration \
	$(NULL)

pcmanfm_LDADD = \
	$(XLIB_LIBS) \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GMODULE_LIBS) \
	$(FM_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(NULL)

noinst_PROGRAMS=xml-purge
xml_purge_SOURCES=xml-purge.c
xml_purge_CFLAGS=$(GIO_CFLAGS)
xml_purge_LDADD=$(GIO_LIBS)
