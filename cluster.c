#include "couchbase.h"
#include "ext/standard/php_var.h"
#include "exception.h"
#include "zap.h"
#include "cluster.h"
#include "opcookie.h"

zap_class_entry cluster_class;
zend_class_entry *cluster_ce;

#define PHP_THISOBJ() zap_fetch_this(cluster_object)

zap_FREEOBJ_FUNC(cluster_free_storage)
{
    cluster_object *obj = zap_get_object(cluster_object, object);

    if (obj->lcb != NULL) {
        lcb_destroy(obj->lcb);
        obj->lcb = NULL;
    }

    zend_object_std_dtor(&obj->std TSRMLS_CC);
    zap_free_object_storage(obj);
}

zap_CREATEOBJ_FUNC(cluster_create_handler)
{
    cluster_object *obj = zap_alloc_object_storage(cluster_object, type);

    zend_object_std_init(&obj->std, type TSRMLS_CC);
    zap_object_properties_init(&obj->std, type);

    obj->lcb = NULL;

    return zap_finalize_object(obj, &cluster_class);
}

typedef struct {
    opcookie_res header;
    zapval bytes;
} opcookie_http_res;

static void http_complete_callback(lcb_http_request_t request, lcb_t instance,
			const void *cookie, lcb_error_t error,
			const lcb_http_resp_t *resp) {
    opcookie_http_res *result = ecalloc(1, sizeof(opcookie_http_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->bytes, resp->v.v0.bytes, resp->v.v0.nbytes);

    opcookie_push((opcookie*)cookie, &result->header);
}

static lcb_error_t proc_http_results(cluster_object *cluster, zval *return_value,
        opcookie *cookie TSRMLS_DC)
{
    opcookie_http_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // Any error should cause everything to fail... for now?
    err = opcookie_get_first_error(cookie);

    if (err == LCB_SUCCESS) {
        // TODO: This could leak with multiple results...  It also copies
        //   which might not be needed...
        FOREACH_OPCOOKIE_RES(opcookie_http_res, res, cookie) {
            zap_zval_stringl_p(return_value,
                zapval_strval_p(&res->bytes), zapval_strlen_p(&res->bytes))
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_http_res, res, cookie) {
        zapval_destroy(res->bytes);
    }

    return err;
}

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
	    throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
		RETURN_NULL();
	}

	if (zdsn) {
		if (Z_TYPE_P(zdsn) == IS_STRING) {
			dsn = estrndup(Z_STRVAL_P(zdsn), Z_STRLEN_P(zdsn));
		} else {
			throw_pcbc_exception("Expected dsn as string", LCB_EINVAL);
			RETURN_NULL();
		}
	}

	if (zname) {
		if (Z_TYPE_P(zname) == IS_STRING) {
			name = estrndup(Z_STRVAL_P(zname), Z_STRLEN_P(zname));
		} else {
			throw_pcbc_exception("Expected bucket name as string", LCB_EINVAL);
			if (dsn) efree(dsn);
			RETURN_NULL();
		}
	}

	if (zpassword) {
		if (Z_TYPE_P(zpassword) == IS_STRING) {
			password = estrndup(Z_STRVAL_P(zpassword), Z_STRLEN_P(zpassword));
		} else {
			throw_pcbc_exception("Expected bucket password as string", LCB_EINVAL);
			if (dsn) efree(dsn);
			if (name) efree(name);
			RETURN_NULL();
		}
	}

	memset(&create_options, 0, sizeof(create_options));
	create_options.version = 3;
	create_options.v.v3.connstr = dsn;
	create_options.v.v3.username = name;
	create_options.v.v3.passwd = password;
	create_options.v.v3.type = LCB_TYPE_CLUSTER;
	err = lcb_create(&instance, &create_options);

	if (dsn) efree(dsn);
	if (name) efree(name);
	if (password) efree(password);

	if (err != LCB_SUCCESS) {
		throw_lcb_exception(err);
		RETURN_NULL();
	}

	lcb_set_http_complete_callback(instance, http_complete_callback);

	data->lcb = instance;
}

PHP_METHOD(Cluster, connect)
{
	cluster_object *data = PHP_THISOBJ();
	lcb_error_t err;

	err = lcb_connect(data->lcb);
	if (err != LCB_SUCCESS) {
		throw_lcb_exception(err);
		RETURN_NULL();
	}

	lcb_wait(data->lcb);

	err = lcb_get_bootstrap_status(data->lcb);
	if (err != LCB_SUCCESS) {
		throw_lcb_exception(err);
	}

	RETURN_NULL();
}

PHP_METHOD(Cluster, http_request)
{
	cluster_object *data = PHP_THISOBJ();
	lcb_http_cmd_t cmd = { 0 };
	opcookie *cookie;
	lcb_http_type_t type;
	lcb_http_method_t method;
	const char *contenttype;
	zval *ztype, *zmethod, *zpath, *zbody, *zcontenttype;
	lcb_error_t err;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzzzz",
				&ztype, &zmethod, &zpath, &zbody, &zcontenttype) == FAILURE) {
	    throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
		RETURN_NULL();
	}

	if (Z_LVAL_P(ztype) == 1) {
		type = LCB_HTTP_TYPE_VIEW;
	} else if (Z_LVAL_P(ztype) == 2) {
		type = LCB_HTTP_TYPE_MANAGEMENT;
	} else {
	    throw_pcbc_exception("Invalid type.", LCB_EINVAL);
		RETURN_NULL();
	}

	if (Z_LVAL_P(zmethod) == 1) {
		method = LCB_HTTP_METHOD_GET;
	} else if (Z_LVAL_P(zmethod) == 2) {
		method = LCB_HTTP_METHOD_POST;
	} else if (Z_LVAL_P(zmethod) == 3) {
		method = LCB_HTTP_METHOD_PUT;
	} else if (Z_LVAL_P(zmethod) == 4) {
		method = LCB_HTTP_METHOD_DELETE;
	} else {
	    throw_pcbc_exception("Invalid method.", LCB_EINVAL);
		RETURN_NULL();
	}

	if (Z_LVAL_P(zcontenttype) == 1) {
		contenttype = "application/json";
	} else if (Z_LVAL_P(zcontenttype) == 2) {
		contenttype = "application/x-www-form-urlencoded";
	} else {
	    throw_pcbc_exception("Invalid content-type.", LCB_EINVAL);
		RETURN_NULL();
	}

	cmd.v.v0.path = Z_STRVAL_P(zpath);
	cmd.v.v0.npath = Z_STRLEN_P(zpath);
	if (Z_TYPE_P(zbody) == IS_STRING) {
		cmd.v.v0.body = Z_STRVAL_P(zbody);
		cmd.v.v0.nbody = Z_STRLEN_P(zbody);
	}
	cmd.v.v0.method = method;
	cmd.v.v0.chunked = 0;
	cmd.v.v0.content_type = contenttype;

	cookie = opcookie_init();

	err = lcb_make_http_request(data->lcb, cookie, type, &cmd, NULL);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->lcb);

        err = proc_http_results(data, return_value, cookie TSRMLS_CC);
    }

    opcookie_destroy(cookie);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

zend_function_entry cluster_methods[] = {
	PHP_ME(Cluster,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(Cluster,  connect,         NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Cluster,  http_request,    NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

void couchbase_init_cluster(INIT_FUNC_ARGS) {
    zap_init_class_entry(&cluster_class, "_CouchbaseCluster",
            cluster_methods);
    cluster_class.create_obj = cluster_create_handler;
    cluster_class.free_obj = cluster_free_storage;
    cluster_ce = zap_register_internal_class(&cluster_class, cluster_object);
}
