dnl $Id: config.m4 3867 2003-08-14 22:29:23Z ted $
dnl config.m4 for extension phpcups

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(phpcups, for phpcups support,
dnl Make sure that the comment is aligned:
[  --with-phpcups             Include phpcups support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(phpcups, whether to enable phpcups support,
dnl Make sure that the comment is aligned:
dnl [  --enable-phpcups           Enable phpcups support])

if test "$PHP_PHPCUPS" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-phpcups -> check with-path
	dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/phpcups.h"  # you most likely want to change this
  dnl if test -r $PHP_PHPCUPS/; then # path given as parameter
  dnl   PHPCUPS_DIR=$PHP_PHPCUPS
  dnl else # search default path list
  dnl   AC_MSG_CHECKING(for phpcups files in default path)
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       PHPCUPS_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$PHPCUPS_DIR"; then
  dnl   AC_MSG_RESULT(not found)
  dnl   AC_MSG_ERROR(Please reinstall the phpcups distribution)
  dnl fi

  dnl # --with-phpcups -> add include path
  PHP_ADD_INCLUDE(/var/httpd2/web2/cups)
  PHP_ADD_INCLUDE(/var/httpd2/web2/cups/cups)

  dnl # --with-phpcups -> chech for lib and symbol presence
  dnl LIBNAME=phpcups # you may want to change this
  dnl LIBSYMBOL=phpcups # you most likely want to change this 
  dnl old_LIBS=$LIBS
  dnl LIBS="$LIBS -L$PHPCUPS_DIR/lib -lm -ldl"
  dnl AC_CHECK_LIB($LIBNAME, $LIBSYMBOL, [AC_DEFINE(HAVE_PHPCUPSLIB,1,[ ])],
	dnl			[AC_MSG_ERROR(wrong phpcups lib version or lib not found)])
  dnl LIBS=$old_LIBS
  dnl
  dnl PHP_SUBST(PHPCUPS_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH("cups", "/var/httpd2/web2/cups/cups", PHPCUPS_SHARED_LIBADD)

  PHP_EXTENSION(phpcups, $ext_shared)
fi
