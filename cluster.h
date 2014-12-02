#ifndef CLUSTER_H_
#define CLUSTER_H_

#include <libcouchbase/couchbase.h>
#include <php.h>

typedef struct cluster_object {
	zend_object std;
	lcb_t lcb;
	zval *error;
} cluster_object;

#endif // CLUSTER_H_
