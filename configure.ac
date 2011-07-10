AC_PREREQ([2.67])

# Initialize build system tools
AC_INIT([geany-zencoding], [0.01], [mbrush@codebrainz.ca])
AM_INIT_AUTOMAKE([foreign silent-rules])
LT_INIT([disable-static])

# Configure build system and tools
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/plugin.c])

# Check for utilities
AC_PROG_CC
AC_PROG_SED

# Check for Python to pre-compile python sources
AC_PATH_PROG([PYTHON], [python], [no])
AC_SUBST([PYTHON])
if [[ x"$PYTHON" = "xno" ]]; then
	AC_MSG_ERROR([Python not found, is it installed?])
fi

# Check for headers, libraries and packages
AC_CHECK_HEADERS([stdio.h string.h assert.h])
PKG_CHECK_MODULES([gtk], [gtk+-2.0])
PKG_CHECK_MODULES([geany], [geany])
AX_PYTHON_DEVEL([>= '2.6'])

# Generate build files
AC_OUTPUT([Makefile
	src/Makefile
	zencoding/Makefile
	zencoding/actions/Makefile
	zencoding/filters/Makefile
	zencoding/interface/Makefile
	zencoding/parser/Makefile
	data/Makefile])
