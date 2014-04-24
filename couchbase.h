#ifndef COUCHBASE_H_
#define COUCHBASE_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "zend_exceptions.h"
#include "php_couchbase.h"

#define PHP_THISOBJ() zend_object_store_get_object(getThis() TSRMLS_CC)

void couchbase_init_exceptions(INIT_FUNC_ARGS);
void couchbase_init_cluster(INIT_FUNC_ARGS);
void couchbase_init_bucket(INIT_FUNC_ARGS);

#endif /* COUCHBASE_H_ */
