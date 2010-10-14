NULL=

SUBDIRS=ui

desktopdir=$(datadir)/applications
desktop_in_files= \
	pcmanfm.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

defconfdir=$(sysconfdir)/xdg/pcmanfm/default
defconf_DATA = \
	pcmanfm.conf \
	$(NULL)

EXTRA_DIST= \
	$(desktop_DATA) \
	$(defconf_DATA) \
	$(NULL)

