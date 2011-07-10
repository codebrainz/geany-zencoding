AC_PREREQ([2.67])

# Initialize build system tools
AC_INIT([geany-zencoding], [0.01], [mbrush@codebrainz.ca])
AM_INIT_AUTOMAKE([foreign silent-rules])
LT_INIT([disable-static])

# Configure build system and tools
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/plugin.c])
AC_CONFIG_HEADER([src/config.h])

# Check for utilities
AC_PROG_CC
AC_PROG_SED

# Check for the py_compilefiles script to pre-compile python sources
AC_PATH_PROG([PY_COMPILEFILES], [py_compilefiles], [no])
AC_SUBST([PY_COMPILEFILES])
if [[ x"$PY_COMPILEFILES" = "xno" ]]; then
	AC_MSG_ERROR([py_compilefiles not found, is Python installed?])
fi

# Check for headers, libraries and packages
AC_CHECK_HEADERS([stdio.h string.h assert.h])
PKG_CHECK_MODULES([gtk], [gtk+-2.0])
PKG_CHECK_MODULES([geany], [geany])
AX_PYTHON_DEVEL([>= '2.6'])

# Generate build files
AC_OUTPUT([Makefile src/Makefile zencoding/Makefile data/Makefile])
