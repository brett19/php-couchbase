#ifndef CLUSTER_H_
#define CLUSTER_H_

#include <php.h>
#include "couchbase.h"
#include "zap.h"

typedef struct cluster_object {
    zap_ZEND_OBJECT_START

    lcb_t lcb;

    zap_ZEND_OBJECT_END
} cluster_object;

#endif // CLUSTER_H_
