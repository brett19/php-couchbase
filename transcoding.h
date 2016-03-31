#ifndef TRANSCODING_H_
#define TRANSCODING_H_

#include <php.h>
#include <libcouchbase/couchbase.h>
#include "zap.h"
#include "bucket.h"

int pcbc_decode_value(bucket_object *bucket, zapval *zvalue,
        zapval *zbytes, zapval *zflags, zapval *zdatatype TSRMLS_DC);

int pcbc_encode_value(bucket_object *bucket, zval *value,
	const void **bytes, lcb_size_t *nbytes, lcb_uint32_t *flags,
	lcb_uint8_t *datatype TSRMLS_DC);

#endif // TRANSCODING_H_
