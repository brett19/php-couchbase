#include <libcouchbase/couchbase.h>
#include <php.h>

int le_cas;

static void cas_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
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
	lcb_cas_t *cas = 0;
	ZEND_FETCH_RESOURCE_NO_RETURN(cas, lcb_cas_t*, &zcas, -1, "CouchbaseCAS", le_cas);
	if (cas) {
		return *cas;
	} else {
		return 0;
	}
}

zval * cas_create(lcb_cas_t value TSRMLS_DC) {
	zval *cas;
	void *cas_data = emalloc(sizeof(lcb_cas_t));
	*((lcb_cas_t*)cas_data) = value;
	MAKE_STD_ZVAL(cas);
	ZEND_REGISTER_RESOURCE(cas, cas_data, le_cas);
	return cas;
}
