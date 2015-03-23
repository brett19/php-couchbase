#include "transcoding.h"

int pcbc_bytes_to_zval(bucket_object *obj, zval **zvalue, const void *bytes,
	lcb_size_t nbytes, lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC) {
	zval zbytes, zflags, zdatatype;
	zval *zparams[] = { &zbytes, &zflags, &zdatatype };

	INIT_ZVAL(zbytes);
	if (nbytes > 0) {
	    ZVAL_STRINGL(&zbytes, bytes, nbytes, 0);
	} else {
	    ZVAL_STRINGL(&zbytes, "", 0, 0);
	}

	INIT_ZVAL(zflags);
	ZVAL_LONG(&zflags, flags);

	INIT_ZVAL(zdatatype);
	ZVAL_LONG(&zdatatype, datatype);

	MAKE_STD_ZVAL(*zvalue);
	if (call_user_function(CG(function_table), NULL, obj->decoder, *zvalue,
		3, zparams TSRMLS_CC) != SUCCESS) {
		return FAILURE;
	}

	return SUCCESS;
}

int pcbc_zval_to_bytes(bucket_object *obj, zval *value,
		const void **bytes, lcb_size_t *nbytes, lcb_uint32_t *flags,
		lcb_uint8_t *datatype TSRMLS_DC) {
	zval zretval, **zpbytes, **zpflags, **zpdatatype;
	zval *zparams[] = { value };
	HashTable *retval;

	if (call_user_function(CG(function_table), NULL, obj->encoder, &zretval,
		1, zparams TSRMLS_CC) != SUCCESS) {
		return FAILURE;
	}

	retval = Z_ARRVAL(zretval);

	if (zend_hash_num_elements(retval) != 3) {
		return FAILURE;
	}

	zend_hash_index_find(retval, 0, (void**)&zpbytes);
	zend_hash_index_find(retval, 1, (void**)&zpflags);
	zend_hash_index_find(retval, 2, (void**)&zpdatatype);

	if (Z_TYPE_PP(zpbytes) != IS_STRING) {
		return FAILURE;
	}
	if (Z_TYPE_PP(zpflags) != IS_LONG) {
		return FAILURE;
	}
	if (Z_TYPE_PP(zpdatatype) != IS_LONG) {
		return FAILURE;
	}

	*nbytes = Z_STRLEN_PP(zpbytes);
	*bytes = estrndup(Z_STRVAL_PP(zpbytes), *nbytes);
	*flags = Z_LVAL_PP(zpflags);
	*datatype = (lcb_uint8_t)Z_LVAL_PP(zpdatatype);

	zval_dtor(&zretval);

	return SUCCESS;
}
