#ifndef BUCKET_H_
#define BUCKET_H_

#include <php.h>
#include "couchbase.h"
#include "zap.h"

typedef struct bucket_object {
    zap_ZEND_OBJECT_START

    zapval encoder;
    zapval decoder;
    zapval prefix;
    pcbc_lcb *conn;

    zap_ZEND_OBJECT_END
} bucket_object;

#endif // BUCKET_H_
