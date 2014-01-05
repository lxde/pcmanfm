NULL=

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/pcmanfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/pcmanfm/ui"\" \
	-DPACKAGE_MODULES_DIR=\""$(libdir)/pcmanfm"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = pcmanfm

pcmanfm_SOURCES = \
	pcmanfm.c \
	app-config.c \
	main-win.c \
	tab-page.c \
	desktop.c \
	volume-manager.c \
	pref.c \
	single-inst.c \
	connect-server.c \
	$(NULL)

EXTRA_DIST= \
	main-win-ui.c \
	desktop-ui.c \
	pcmanfm.h \
	app-config.h \
	main-win.h \
	tab-page.h \
	desktop.h \
	volume-manager.h \
	pref.h \
	single-inst.h \
	connect-server.h \
	gseal-gtk-compat.h \
	$(NULL)

include_HEADERS = pcmanfm-modules.h

pcmanfm_CFLAGS = \
	$(XLIB_CFLAGS) \
	$(FM_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall \
	-Werror-implicit-function-declaration \
	$(NULL)

pcmanfm_LDADD = \
	$(XLIB_LIBS) \
	$(FM_LIBS) \
	$(NULL)
