#ifndef PHP_COUCHBASE_H_
#define PHP_COUCHBASE_H_

#define PHP_COUCHBASE_VERSION "2.0.5"
#define PHP_COUCHBASE_EXTNAME "couchbase"

extern zend_module_entry couchbase_module_entry;
#define phpext_couchbase_ptr &couchbase_module_entry

#endif
