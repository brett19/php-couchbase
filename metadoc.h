#ifndef METADOC_H_
#define METADOC_H_

#include <php.h>
#include <libcouchbase/couchbase.h>
#include "bucket.h"

void couchbase_init_metadoc(INIT_FUNC_ARGS);

int make_metadoc(zval *doc, zapval *value, zapval *flags, zapval *cas TSRMLS_DC);
int make_metadoc_error(zval *doc, lcb_error_t err TSRMLS_DC);

#endif // METADOC_H_
