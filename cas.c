#include <libcouchbase/couchbase.h>
#include <php.h>
#include "cas.h"

int le_cas;

static zap_DTORRES_FUNC(cas_dtor)
{
    lcb_cas_t *cas_data = (lcb_cas_t*)rsrc->ptr;
    if (cas_data) {
        efree(cas_data);
    }
}

void couchbase_init_cas(INIT_FUNC_ARGS) {
    le_cas = zend_register_list_destructors_ex(cas_dtor, NULL, "CouchbaseCAS", module_number);
}

lcb_cas_t cas_retrieve(zval * zcas TSRMLS_DC) {
    lcb_cas_t *cas = (lcb_cas_t*)zap_fetch_resource(zcas, "CouchbaseCAS", le_cas);
    if (cas) {
        return *cas;
    } else {
        return 0;
    }
}

void cas_create(zapval *casout, lcb_cas_t value TSRMLS_DC) {
    void *cas_data = emalloc(sizeof(lcb_cas_t));
    *((lcb_cas_t*)cas_data) = value;
    zapval_alloc_res(*casout, cas_data, le_cas);
}
