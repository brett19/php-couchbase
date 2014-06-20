#include <libcouchbase/couchbase.h>
#include "couchbase.h"
#include "exception.h"

zend_object_handlers cluster_handlers;


typedef struct cluster_object {
    zend_object std;
    lcb_t lcb;
    zval *error;
} cluster_object;

void cluster_free_storage(void *object TSRMLS_DC)
{
	cluster_object *obj = (cluster_object *)object;

    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    efree(obj);
}

zend_object_value cluster_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    cluster_object *obj = (cluster_object *)emalloc(sizeof(cluster_object));
    memset(obj, 0, sizeof(cluster_object));
    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
    zend_hash_copy(obj->std.properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
    object_properties_init(&obj->std, type);
#endif

    retval.handle = zend_objects_store_put(obj, NULL,
        cluster_free_storage, NULL TSRMLS_CC);
    retval.handlers = &cluster_handlers;

    return retval;
}


static int pcbc_wait(lcb_t instance TSRMLS_DC)
{
    cluster_object *data = (cluster_object*)lcb_get_cookie(instance);
    data->error = NULL;

    lcb_wait(instance);

    if (data->error) {
        zend_throw_exception_object(data->error TSRMLS_CC);
        data->error = NULL;
        return 0;
    }

    return 1;
}


zend_class_entry *cluster_ce;

PHP_METHOD(Cluster, __construct)
{
	cluster_object *data = PHP_THISOBJ();
	zval *zdsn = NULL;
	zval *zname = NULL;
	zval *zpassword = NULL;
	char *dsn = NULL;
	char *name = NULL;
	char *password = NULL;
	lcb_error_t err;
	lcb_t instance;
	struct lcb_create_st create_options;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz",
	        &zdsn, &zname, &zpassword) == FAILURE) {
		RETURN_NULL();
	}

	if (zdsn) {
        if (Z_TYPE_P(zdsn) == IS_STRING) {
            dsn = estrndup(Z_STRVAL_P(zdsn), Z_STRLEN_P(zdsn));
        } else {
            php_printf("Expected dsn as string\n");
            RETURN_NULL();
        }
    }

    if (zname) {
        if (Z_TYPE_P(zname) == IS_STRING) {
            name = estrndup(Z_STRVAL_P(zname), Z_STRLEN_P(zname));
        } else {
            php_printf("Expected bucket name as string\n");
            if (dsn) efree(dsn);
            RETURN_NULL();
        }
    }

    if (zpassword) {
        if (Z_TYPE_P(zpassword) == IS_STRING) {
            password = estrndup(Z_STRVAL_P(zpassword), Z_STRLEN_P(zpassword));
        } else {
            php_printf("Expected bucket password as string\n");
            if (dsn) efree(dsn);
            if (name) efree(name);
            RETURN_NULL();
        }
    }

	memset(&create_options, 0, sizeof(create_options));
    create_options.version = 3;
    create_options.v.v3.dsn = dsn;
    create_options.v.v3.username = name;
    create_options.v.v3.passwd = password;
    err = lcb_create(&instance, &create_options);

    if (dsn) efree(dsn);
    if (name) efree(name);
    if (password) efree(password);

    if (err != LCB_SUCCESS) {
        zend_throw_exception_object(create_lcb_exception(err TSRMLS_CC) TSRMLS_CC);
        RETURN_NULL();
    }

	lcb_set_cookie(instance, data);

	data->lcb = instance;
}

PHP_METHOD(Cluster, connect)
{
	cluster_object *data = PHP_THISOBJ();
	lcb_error_t err;

    err = lcb_connect(data->lcb);
    if (err != LCB_SUCCESS) {
        zend_throw_exception_object(create_lcb_exception(err TSRMLS_CC) TSRMLS_CC);
        RETURN_NULL();
    }

    pcbc_wait(data->lcb TSRMLS_CC);
}

zend_function_entry cluster_methods[] = {
    PHP_ME(Cluster,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Cluster,  connect,         NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void couchbase_init_cluster(INIT_FUNC_ARGS) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "_CouchbaseCluster", cluster_methods);
    ce.create_object = cluster_create_handler;
    cluster_ce = zend_register_internal_class(&ce TSRMLS_CC);

    memcpy(&cluster_handlers,
    		zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    cluster_handlers.clone_obj = NULL;
}
