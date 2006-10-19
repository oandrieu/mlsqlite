AC_INIT(mlSQLite, 0.5.0, [<oandrieu+mlsqlite@gmail.com>])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([sqlite3.ml])
dnl AC_PREFIX_PROGRAM(ocamlc)

# OCaml configuration
AC_PROG_OCAML
AC_PROG_OCAML_TOOLS
AC_PROG_FINDLIB

# Wether to build a shared stub library or not
AC_ARG_VAR(STATIC, [wether to build a shared stub library or not])
AC_ARG_ENABLE(shared, AS_HELP_STRING([--disable-shared],[wether to build a shared stub library or not]))
if test "$enable_shared" = no ; then STATIC=yes ; fi

# Sqlite3 config
PKG_CHECK_MODULES(SQLITE3, sqlite3)

# checking sqlite3 particular functions: 
# sqlite has the habit of having prototypes in the .h
# that don't end up in the final library. *Very* annoying.
CPPFLAGS="$CPPFLAGS $SQLITE3_CFLAGS"
LIBS="$LIBS $SQLITE3_LIBS"
AC_CHECK_FUNCS(sqlite3_get_autocommit \
               sqlite3_sleep \
               sqlite3_bind_value \
               sqlite3_clear_bindings \
               sqlite3_progress_handler \
               sqlite3_complete)

AC_OUTPUT(config.make)
