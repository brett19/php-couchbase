#ifndef CAS_H_
#define CAS_H_

#include <php.h>
#include <libcouchbase/couchbase.h>
#include "zap.h"

void couchbase_init_cas(INIT_FUNC_ARGS);
lcb_cas_t cas_retrieve(zval * zcas TSRMLS_DC);
void cas_create(zapval *casout, lcb_cas_t value TSRMLS_DC);

#define alloc_cas(z, v) \
    cas_create(&z, v)

#endif // CAS_H_
