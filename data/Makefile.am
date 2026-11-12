NULL=

SUBDIRS=ui

man_MANS=pcmanfm.1

desktopdir=$(datadir)/applications
desktop_in_files= \
	pcmanfm.desktop.in \
	pcmanfm-desktop-pref.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
SUFFIXES = .desktop.in .desktop
.desktop.in.desktop:
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

defconfdir=$(sysconfdir)/xdg/pcmanfm/default
defconf_DATA = \
	pcmanfm.conf \
	$(NULL)

EXTRA_DIST= \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(defconf_DATA) \
	$(NULL)
