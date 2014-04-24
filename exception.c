#include "couchbase.h"
#include <libcouchbase/couchbase.h>

zend_class_entry *default_exception_ce;
zend_class_entry *cb_exception_ce;

zval * create_exception(zend_class_entry *exception_ce, const char *message, long code TSRMLS_DC) {
	zend_class_entry *default_exception_ce = zend_exception_get_default(TSRMLS_C);
	zval *ex;

	MAKE_STD_ZVAL(ex);
	object_init_ex(ex, exception_ce);

	if (message) {
		zend_update_property_string(default_exception_ce, ex, "message", sizeof("message")-1, message TSRMLS_CC);
	}
	if (code) {
		zend_update_property_long(default_exception_ce, ex, "code", sizeof("code")-1, code TSRMLS_CC);
	}

	return ex;
}

zval * create_lcb_exception(long code TSRMLS_DC) {
	const char *str = lcb_strerror(NULL, (lcb_error_t)code);
	return create_exception(cb_exception_ce, str, code TSRMLS_CC);
}

#define setup(var, name, parent)                                                                                \
        do {                                                                                                                                \
                zend_class_entry cbe;                                                                                        \
                INIT_CLASS_ENTRY(cbe, name, NULL);                                                                \
                var = zend_register_internal_class_ex(&cbe, parent, NULL TSRMLS_CC); \
        } while(0)

void couchbase_init_exceptions(INIT_FUNC_ARGS) {
#if ZEND_MODULE_API_NO >= 20060613
	default_exception_ce = (zend_class_entry *)zend_exception_get_default(TSRMLS_C);
#else
	default_exception_ce = (zend_class_entry *)zend_exception_get_default();
#endif

	setup(cb_exception_ce, "CouchbaseException", default_exception_ce);
}
