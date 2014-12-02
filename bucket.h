#ifndef BUCKET_H_
#define BUCKET_H_

#include <php.h>
#include "couchbase.h"

typedef struct bucket_object {
	zend_object std;
	zval *error;
	zval *encoder;
	zval *decoder;
	zval *prefix;

	pcbc_lcb *conn;
} bucket_object;

#endif // BUCKET_H_
