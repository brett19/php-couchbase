#ifndef METADOC_H_
#define METADOC_H_

#include <php.h>
#include <libcouchbase/couchbase.h>
#include "bucket.h"

void couchbase_init_metadoc(INIT_FUNC_ARGS);

int metadoc_create(zval *doc, zval *value, lcb_cas_t cas,
	lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC);

int metadoc_from_error(zval *doc, zval *zerror TSRMLS_DC);

int metadoc_from_long(zval *doc, lcb_U64 value,
	lcb_cas_t cas, lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC);

int metadoc_from_bytes(bucket_object *obj, zval *doc, const void *bytes,
	lcb_size_t nbytes, lcb_cas_t cas, lcb_uint32_t flags,
	lcb_uint8_t datatype TSRMLS_DC);

#endif // METADOC_H_