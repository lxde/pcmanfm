#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([pcmanfm], [1.2.2], [http://pcmanfm.sf.net/])
AM_INIT_AUTOMAKE([-Wall -Werror foreign no-dist-gzip dist-xz])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_SEARCH_LIBS([floor], [m])

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

AC_ARG_ENABLE(debug,
    [AC_HELP_STRING([--enable-debug],
        [build pcmanfm with debug support @<:@default=no@:>@])],
    [enable_debug="${enableval}"],
    [enable_debug=no])

if test "$enable_debug" = "yes"; then
    # turn on debug and disable optimization
    CPPFLAGS="$CPPFLAGS -DG_ENABLE_DEBUG -O0 -g"
    case "$CC" in
    gcc*)
        CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
        ;;
    *)
        ;;
    esac
    dnl Be more strict on compatibility
    CPPFLAGS="$CPPFLAGS -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=700"
    dnl debug should enforce cast checks
    cast_checks=yes
else
    # turn off glib debug checks
    # G_DISABLE_CAST_CHECKS is already handled by $(G_CAST_CHECKS)
    CPPFLAGS="$CPPFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
fi

G_CAST_CHECKS=""
if test x"$cast_checks" = "xno"; then
    G_CAST_CHECKS="-DG_DISABLE_CAST_CHECKS"
fi
AC_SUBST(G_CAST_CHECKS)

# intltool
IT_PROG_INTLTOOL([0.40.0])

dnl libgtk version
AC_ARG_WITH(gtk,
    [AC_HELP_STRING([--with-gtk=VER],
        [choose Gtk+ version for libfm-gtk build (default=2)])],
    [ac_with_gtk="${withval}"],
    [ac_with_gtk=2])

# Checks for libraries.

PKG_CHECK_MODULES(XLIB, "x11")
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

gio_modules="gthread-2.0 gio-unix-2.0 >= 2.18.0"
PKG_CHECK_MODULES(GIO, [$gio_modules])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

fm_modules="$gio_modules glib-2.0 pango >= 1.20.0 libfm >= 1.0"
case "$ac_with_gtk" in
    3|3.*)
        fm_modules="$fm_modules gtk+-3.0 libfm-gtk3 >= 1.0.1"
        ;;
    *)
        fm_modules="$fm_modules gtk+-2.0 libfm-gtk >= 1.0.1"
        ;;
esac
PKG_CHECK_MODULES(FM, [$fm_modules])
AC_SUBST(FM_CFLAGS)
AC_SUBST(FM_LIBS)

GETTEXT_PACKAGE=pcmanfm
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

dnl Fix invalid sysconfdir when --prefix=/usr
if test `eval "echo $sysconfdir"` = /usr/etc
then
    sysconfdir=/etc
fi

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    po/Makefile.in
    data/Makefile
    data/pcmanfm.1
    data/ui/Makefile
    data/ui/about.glade
])

AC_OUTPUT

echo Ready to make pcmanfm
