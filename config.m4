PHP_ARG_WITH(couchbase, whether to enable Couchbase support,
[ --with-couchbase   Include Couchbase support])

if test "$PHP_COUCHBASE" != "no"; then
  if test -r $PHP_COUCHBASE/include/libcouchbase/couchbase.h; then
    LIBCOUCHBASE_DIR=$PHP_COUCHBASE
  else
    AC_MSG_CHECKING(for libcouchbase in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/libcouchbase/couchbase.h; then
        LIBCOUCHBASE_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  if test -z "$LIBCOUCHBASE_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the libcouchbase distribution -
                 libcouchbase.h should be <libcouchbase-dir>/include and
                 libcouchbase.a should be in <libcouchbase-dir>/lib)
  fi
  PHP_ADD_INCLUDE($LIBCOUCHBASE_DIR/include)
  
  PHP_SUBST(COUCHBASE_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(couchbase, $LIBCOUCHBASE_DIR/lib, 
               COUCHBASE_SHARED_LIBADD)

  AC_DEFINE(HAVE_COUCHBASE, 1, [Whether you have Couchbase])

  ifdef([PHP_ADD_EXTENDION_DEP], [
	PHP_ADD_EXTENSION_DEP(couchbase, json)
  ]) 

  PHP_SUBST(COUCHBASE_SHARED_LIBADD)
  PHP_NEW_EXTENSION(couchbase, \
	bucket.c \
	cas.c \
	cluster.c \
	couchbase.c \
	exception.c \
	metadoc.c \
	transcoding.c \
	fastlz/fastlz.c \
  , $ext_shared)
fi