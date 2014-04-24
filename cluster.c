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
	zval *zhosts = NULL;
	zval *zname = NULL;
	zval *zpassword = NULL;
	zval *zccache = NULL;
	char *hosts = NULL;
	char *name = NULL;
	char *password = NULL;
	char *ccache = NULL;
	lcb_error_t err;
	lcb_t instance;
	struct lcb_create_st create_options;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzzz",
	        &zhosts, &zname, &zpassword, &zccache) == FAILURE) {
		RETURN_NULL();
	}

	if (zhosts) {
		if (Z_TYPE_P(zhosts) == IS_STRING) {
		    hosts = estrndup(Z_STRVAL_P(zhosts), Z_STRLEN_P(zhosts));
		} else if (Z_TYPE_P(zhosts) == IS_ARRAY) {
			zval **host = NULL;
			HashTable *hosts_hash = Z_ARRVAL_P(zhosts);
			HashPosition hosts_ptr;
			int ii;

			spprintf(&hosts, 0, "");

			for(ii = 0, zend_hash_internal_pointer_reset_ex(hosts_hash, &hosts_ptr);
					zend_hash_get_current_data_ex(hosts_hash, (void**) &host, &hosts_ptr) == SUCCESS;
					zend_hash_move_forward_ex(hosts_hash, &hosts_ptr), ++ii) {

				if (Z_TYPE_PP(host) == IS_STRING) {
					char *tmp_host_list = NULL;
					if (ii > 0) {
						spprintf(&tmp_host_list, 0, "%s;%s", hosts, Z_STRVAL_PP(host));
					} else {
						spprintf(&tmp_host_list, 0, "%s", Z_STRVAL_PP(host));
					}
					efree(hosts);
					hosts = tmp_host_list;
				} else {
					php_printf("Expected proper host in array\n");
					efree(hosts);
					RETURN_NULL();
				}

			}
		} else {
			php_printf("Expected proper hosts\n");
			RETURN_NULL();
		}
	} else {
		spprintf(&hosts, 0, "localhost:8091");
	}

    if (zname) {
        if (Z_TYPE_P(zname) == IS_STRING) {
            name = estrndup(Z_STRVAL_P(zname), Z_STRLEN_P(zname));
        } else {
            php_printf("Expected bucket name as string\n");
            if (hosts) efree(hosts);
            RETURN_NULL();
        }
    }

    if (zpassword) {
        if (Z_TYPE_P(zpassword) == IS_STRING) {
            password = estrndup(Z_STRVAL_P(zpassword), Z_STRLEN_P(zpassword));
        } else {
            php_printf("Expected bucket password as string\n");
            if (hosts) efree(hosts);
            if (name) efree(name);
            RETURN_NULL();
        }
    }

    if (zccache) {
        if (Z_TYPE_P(zccache) == IS_STRING) {
            ccache = estrndup(Z_STRVAL_P(zccache), Z_STRLEN_P(zccache));
        } else {
            php_printf("Expected config cache path as string\n");
            if (hosts) efree(hosts);
            if (name) efree(name);
            if (password) efree(password);
            RETURN_NULL();
        }
    }


	memset(&create_options, 0, sizeof(create_options));
	create_options.v.v1.host = hosts;
	create_options.v.v1.bucket = name;
	create_options.v.v1.passwd = password;
	create_options.v.v1.type = LCB_TYPE_CLUSTER;

    if (!ccache) {
        err = lcb_create(&instance, &create_options);
    } else {
        struct lcb_cached_config_st cache_config = { 0 };
		cache_config.createopt = create_options;
		cache_config.cachefile = ccache;

        err = lcb_create_compat(
            LCB_CACHED_CONFIG,
            &create_options,
            &instance,
            NULL);
    }

    if (hosts) efree(hosts);
    if (name) efree(name);
    if (password) efree(password);
    if (ccache) efree(ccache);

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
