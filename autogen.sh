#! /bin/sh
AC_VERSION=

AUTOPOINT=${AUTOPOINT:-autopoint}
AUTOMAKE=${AUTOMAKE:-automake}
AM_INSTALLED_VERSION=$($AUTOMAKE --version | sed -e '2,$ d' -e 's/.* \([0-9]*\.[0-9]*\).*/\1/')

# FIXME: we need a better way for version check later.
case "$AM_INSTALLED_VERSION" in
    1.1[0-9])
	;;
    *)
	echo
	echo "You must have automake 1.10 or newer installed."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
	exit 1
	;;
esac

set -x

if [ "x${ACLOCAL_DIR}" != "x" ]; then
  ACLOCAL_ARG=-I ${ACLOCAL_DIR}
fi

$AUTOPOINT --force
${ACLOCAL:-aclocal$AM_VERSION} -I m4 ${ACLOCAL_ARG}
${AUTOHEADER:-autoheader$AC_VERSION} --force
$AUTOMAKE --add-missing --copy --include-deps
${AUTOCONF:-autoconf$AC_VERSION}

rm -rf autom4te.cache
