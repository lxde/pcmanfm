#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([pcmanfm], [0.9.5], [http://pcmanfm.sf.net/])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_LIBTOOL
AM_PROG_CC_STDC
AC_HEADER_STDC

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Large file support
AC_ARG_ENABLE([largefile],
    AS_HELP_STRING([--enable-largefile],
        [enable Large file support (default: yes)]),
    largefile=$enableval, largefile="yes")

if test x"$largefile" = x"yes"; then
  CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64"
  AC_DEFINE(_LARGEFILE64_SOURCE, 1, [Whether to enable large file support])
  AC_DEFINE(_FILE_OFFSET_BITS, 64, [File offset bits])
fi

AC_ARG_ENABLE(
    [cast-checks],
    AS_HELP_STRING([--enable-cast-checks],
                   [enable Glib casting checks (default: disable)]),
    cast_checks=$enableval, cast_checks="no")
G_CAST_CHECKS=""
if test x"$cast_checks" = "xno"; then
    G_CAST_CHECKS="-DG_DISABLE_CAST_CHECKS"
fi
AC_SUBST(G_CAST_CHECKS)

AC_ARG_ENABLE(debug,
	[AC_HELP_STRING([--enable-debug],
		[build pcmanfm with debug support @<:@default=no@:>@])],
	[enable_debug="${enableval}"],
	[enable_debug=no]
)
if test "$enable_debug" = "yes"; then
    # turn on debug and disable optimization
    CPPFLAGS="$CPPFLAGS -DG_ENABLE_DEBUG -O0 -g"
else
    # turn off glib debug checks
    # G_DISABLE_CAST_CHECKS is already handled by $(G_CAST_CHECKS)
    CPPFLAGS="$CPPFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
fi

# intltool
IT_PROG_INTLTOOL([0.40.0])

# Checks for libraries.

PKG_CHECK_MODULES(XLIB, "x11")
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

gtk_modules="gtk+-2.0 >= 2.16.0"
PKG_CHECK_MODULES(GTK, [$gtk_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

gio_modules="gthread-2.0 gio-unix-2.0"
PKG_CHECK_MODULES(GIO, [$gio_modules])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

gmodule_modules="gmodule-2.0"
PKG_CHECK_MODULES(GMODULE, [$gmodule_modules])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

fm_modules="libfm-gtk >= 0.1.10"
PKG_CHECK_MODULES(FM, [$fm_modules])
AC_SUBST(FM_CFLAGS)
AC_SUBST(FM_LIBS)

menu_modules="libmenu-cache >= 0.3.2"
PKG_CHECK_MODULES(MENU_CACHE, [$menu_modules])
AC_SUBST(MENU_CACHE_CFLAGS)
AC_SUBST(MENU_CACHE_LIBS)

GETTEXT_PACKAGE=pcmanfm2
AC_SUBST(GETTEXT_PACKAGE)

# Generate po/LINGUAS on the fly rather than relying on translators
# to maintain it manually. This also overcome the problem that Transifex
# cannot add a language to po/LINGUAS if a new po file is submitted.
rm -f $srcdir/po/LINGUAS
for po_file in `ls $srcdir/po/*.po | sort`;
do
    lang=`echo "$po_file" | sed "s|.*/po/\(.*\)\.po|\1|g"`
    echo $lang >> $srcdir/po/LINGUAS
done

AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    po/Makefile.in
    data/Makefile
    data/ui/Makefile
    data/ui/about.glade
    data/pcmanfm2.desktop
])

AC_OUTPUT

echo Ready to make pcmanfm
