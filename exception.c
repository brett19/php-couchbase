#include "couchbase.h"
#include <libcouchbase/couchbase.h>
#include "zap.h"

zend_class_entry *default_exception_ce;
zend_class_entry *cb_exception_ce;

void make_exception(zapval *ex, zend_class_entry *exception_ce, const char *message, long code TSRMLS_DC) {
    zapval_alloc(*ex);
    object_init_ex(zapval_zvalptr_p(ex), cb_exception_ce);

    if (message) {
        zend_update_property_string(
                cb_exception_ce,
                zapval_zvalptr_p(ex),
                "message", sizeof("message")-1,
                message TSRMLS_CC);
    }
    if (code) {
        zend_update_property_long(
                cb_exception_ce,
                zapval_zvalptr_p(ex),
                "code", sizeof("code")-1,
                code TSRMLS_CC);
    }
}

void make_pcbc_exception(zapval *ex, const char *message, long code TSRMLS_DC) {
    make_exception(ex, cb_exception_ce, message, code TSRMLS_CC);
}

void make_lcb_exception(zapval *ex, long code, const char *msg TSRMLS_DC) {
    if (msg) {
        return make_exception(ex, cb_exception_ce, msg, code TSRMLS_CC);
    } else {
        const char *str = lcb_strerror(NULL, (lcb_error_t)code);
        return make_exception(ex, cb_exception_ce, str, code TSRMLS_CC);
    }
}

#define setup(var, name, parent) \
    do { \
        zend_class_entry cbe; \
        INIT_CLASS_ENTRY(cbe, name, NULL); \
        var = zap_zend_register_internal_class_ex(&cbe, parent); \
    } while(0)

void couchbase_init_exceptions(INIT_FUNC_ARGS) {
    default_exception_ce = (zend_class_entry *)zap_zend_exception_get_default();

    setup(cb_exception_ce, "CouchbaseException", default_exception_ce);
}
