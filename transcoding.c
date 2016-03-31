#include "transcoding.h"
#include "zap.h"

int pcbc_decode_value(bucket_object *bucket, zapval *zvalue,
        zapval *zbytes, zapval *zflags, zapval *zdatatype TSRMLS_DC) {
    zapval zparams[] = { *zbytes, *zflags, *zdatatype };

    if (call_user_function(CG(function_table), NULL,
            zapval_zvalptr(bucket->decoder), zapval_zvalptr_p(zvalue),
            3, zparams TSRMLS_CC) != SUCCESS)
    {
        return FAILURE;
    }

    return SUCCESS;
}

int pcbc_encode_value(bucket_object *bucket, zval *value,
        const void **bytes, lcb_size_t *nbytes, lcb_uint32_t *flags,
        lcb_uint8_t *datatype TSRMLS_DC) {
    zapval *zpbytes, *zpflags, *zpdatatype;
    zapval zretval;
    HashTable *retval;

    zapval_alloc_null(zretval);

    if (call_user_function(CG(function_table), NULL,
            zapval_zvalptr(bucket->encoder), zapval_zvalptr(zretval),
            1, zapvalptr_from_zvalptr(value) TSRMLS_CC) != SUCCESS) {
        zapval_destroy(zretval);
        return FAILURE;
    }

    if (!zapval_is_array(zretval)) {
        zapval_destroy(zretval);
        return FAILURE;
    }

    retval = zapval_arrval(zretval);

    if (zend_hash_num_elements(retval) != 3) {
        zapval_destroy(zretval);
        return FAILURE;
    }

    zpbytes = zap_hash_index_find(retval, 0);
    zpflags = zap_hash_index_find(retval, 1);
    zpdatatype = zap_hash_index_find(retval, 2);

    if (!zapval_is_string_p(zpbytes)) {
        zapval_destroy(zretval);
        return FAILURE;
    }
    if (!zapval_is_long_p(zpflags)) {
        zapval_destroy(zretval);
        return FAILURE;
    }
    if (!zapval_is_long_p(zpdatatype)) {
        zapval_destroy(zretval);
        return FAILURE;
    }

    *nbytes = zapval_strlen_p(zpbytes);
    *bytes = estrndup(zapval_strval_p(zpbytes), *nbytes);
    *flags = zapval_lval_p(zpflags);
    *datatype = (lcb_uint8_t)zapval_lval_p(zpdatatype);

    zapval_destroy(zretval);
    return SUCCESS;
}
