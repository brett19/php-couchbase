#include <libcouchbase/couchbase.h>
#include "couchbase.h"
#include "ext/standard/php_var.h"
#include "exception.h"
#include "datainfo.h"
#include "paramparser.h"

#define PHP_CAS_RES_NAME "CAS"

enum durability_t {
	PERSISTTO_ONE = 1,
	PERSISTTO_TWO = 2,
	PERSISTTO_THREE = 4,
	PERSISTTO_MASTER = PERSISTTO_ONE,
	REPLICATETO_ONE = 1 << 4,
	REPLICATETO_TWO = 2 << 4,
	REPLICATETO_THREE = 4 << 4
}

#define PCBC_CHECK_ZVAL(v,t,m) \
	if (v && Z_TYPE_P(v) != t) { \
		/*Throw some exception*/ \
		RETURN_NULL();\
	}

// This is not needed now that the bytes are loaded out of zval's
//   which will be garbage collected anyways.
#define PCBC_FREE_CMDV0BYTES(_cmd, _num_cmds)

int le_cas;

zend_class_entry *metadoc_ce;

zend_function_entry metadoc_methods[] = {
    {NULL, NULL, NULL}
};


zend_object_handlers bucket_handlers;

typedef struct bucket_connection {
    char *key;
    lcb_t lcb;

    void *next;
} bucket_connection;
bucket_connection *first_conn;
bucket_connection *last_conn;

typedef struct bucket_object {
    zend_object std;
    zval *error;
    zval *encoder;
    zval *decoder;
    zval *prefix;

    bucket_connection *conn;
} bucket_object;

void bucket_free_storage(void *object TSRMLS_DC)
{
	bucket_object *obj = (bucket_object *)object;

    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    efree(obj);
}

zend_object_value bucket_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    bucket_object *obj = (bucket_object *)emalloc(sizeof(bucket_object));
    memset(obj, 0, sizeof(bucket_object));
    obj->std.ce = type;
    obj->conn = NULL;

    MAKE_STD_ZVAL(obj->encoder);
    ZVAL_STRING(obj->encoder, "", 1);
    MAKE_STD_ZVAL(obj->decoder);
    ZVAL_STRING(obj->decoder, "", 1);
    MAKE_STD_ZVAL(obj->prefix);
    ZVAL_STRING(obj->prefix, "", 1);

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
#if PHP_VERSION_ID < 50399
    zend_hash_copy(obj->std.properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
#else
    object_properties_init(&obj->std, type);
#endif

    retval.handle = zend_objects_store_put(obj, NULL,
    		bucket_free_storage, NULL TSRMLS_CC);
    retval.handlers = &bucket_handlers;

    return retval;
}

typedef struct {
	int mapped;
	int remaining;
	zval *retval;
} op_cookie;

#define MAKE_STD_COOKIE(cookie, return_value, ismapped) { \
	cookie = emalloc(sizeof(op_cookie)); \
	cookie->retval = return_value; \
	if (ismapped) { \
		array_init(cookie->retval); \
	} else { \
		ZVAL_NULL(cookie->retval); \
	} }

static void error_callback(lcb_t instance, lcb_error_t error, const char *errinfo)
{
	bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
	TSRMLS_FETCH();
	data->error = create_lcb_exception(error TSRMLS_CC);
}

static int pcbc_bytes_to_zval(bucket_object *obj, zval **zvalue, const void *bytes,
		lcb_size_t nbytes, lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC) {
	zval zretval, zbytes, zflags, zdatatype;
	zval *zparams[] = { &zbytes, &zflags, &zdatatype };

	INIT_ZVAL(zbytes);
	ZVAL_STRINGL(&zbytes, bytes, nbytes, 0);

	INIT_ZVAL(zflags);
	ZVAL_LONG(&zflags, flags);

	INIT_ZVAL(zdatatype);
	ZVAL_LONG(&zdatatype, datatype);

	MAKE_STD_ZVAL(*zvalue);
	if (call_user_function(CG(function_table), NULL, obj->decoder, *zvalue,
	        3, zparams TSRMLS_CC) != SUCCESS) {
	    return FAILURE;
	}

	return SUCCESS;
}

static int pcbc_zval_to_bytes(bucket_object *obj, zval *value,
        const void **bytes, lcb_size_t *nbytes, lcb_uint32_t *flags,
		lcb_uint8_t *datatype TSRMLS_DC) {
    zval zretval, **zpbytes, **zpflags, **zpdatatype;
    zval *zparams[] = { value };
    HashTable *retval;

    if (call_user_function(CG(function_table), NULL, obj->encoder, &zretval,
            1, zparams TSRMLS_CC) != SUCCESS) {
        return FAILURE;
    }

    retval = Z_ARRVAL(zretval);

    if (zend_hash_num_elements(retval) != 3) {
        return FAILURE;
    }

    zend_hash_index_find(retval, 0, (void**)&zpbytes);
    zend_hash_index_find(retval, 1, (void**)&zpflags);
    zend_hash_index_find(retval, 2, (void**)&zpdatatype);

    if (Z_TYPE_PP(zpbytes) != IS_STRING) {
        return FAILURE;
    }
    if (Z_TYPE_PP(zpflags) != IS_LONG) {
        return FAILURE;
    }
    if (Z_TYPE_PP(zpdatatype) != IS_LONG) {
        return FAILURE;
    }

    *bytes = Z_STRVAL_PP(zpbytes);
    *nbytes = Z_STRLEN_PP(zpbytes);
    *flags = Z_LVAL_PP(zpflags);
    *datatype = Z_LVAL_PP(zpdatatype);

    return SUCCESS;
}

lcb_cas_t read_cas(zval * zcas TSRMLS_DC) {
	lcb_cas_t *cas = 0;
	ZEND_FETCH_RESOURCE_NO_RETURN(cas, lcb_cas_t*, &zcas, -1, PHP_CAS_RES_NAME, le_cas);
	if (cas) {
		return *cas;
	} else {
		return 0;
	}
}

zval * make_cas(lcb_cas_t value TSRMLS_DC) {
	zval *cas;
	void *cas_data = emalloc(sizeof(lcb_cas_t));
	*((lcb_cas_t*)cas_data) = value;
	MAKE_STD_ZVAL(cas);
	ZEND_REGISTER_RESOURCE(cas, cas_data, le_cas);
	return cas;
}

static int make_metadoc(zval *doc, zval *value, lcb_cas_t cas,
		lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC) {
	zval *zcas, *zflags;

	object_init_ex(doc, metadoc_ce);

	if (value) {
		zend_update_property(metadoc_ce, doc, "value", sizeof("value")-1, value TSRMLS_CC);
	}

	MAKE_STD_ZVAL(zflags);
	ZVAL_LONG(zflags, flags);
	zend_update_property(metadoc_ce, doc, "flags", sizeof("flags")-1, zflags TSRMLS_CC);

	zcas = make_cas(cas TSRMLS_CC);
	zend_update_property(metadoc_ce, doc, "cas", sizeof("cas")-1, zcas TSRMLS_CC);

	return SUCCESS;
}

static int make_metadoc_long(zval *doc, lcb_int32_t value,
		lcb_cas_t cas, lcb_uint32_t flags, lcb_uint8_t datatype TSRMLS_DC) {
	zval *zvalue;
	MAKE_STD_ZVAL(zvalue);
	ZVAL_LONG(zvalue, value);

	return make_metadoc(doc, zvalue, cas, flags, datatype TSRMLS_CC);
}

static int make_metadoc_bytes(bucket_object *obj, zval *doc, const void *bytes,
        lcb_size_t nbytes, lcb_cas_t cas, lcb_uint32_t flags,
        lcb_uint8_t datatype TSRMLS_DC) {
	zval *zvalue;
	pcbc_bytes_to_zval(obj, &zvalue, bytes, nbytes, flags, datatype TSRMLS_CC);

	return make_metadoc(doc, zvalue, cas, flags, datatype TSRMLS_CC);
}

zval * cookie_get_doc(const op_cookie *cookie, const char *key, uint key_len) {
	// Maximum server keylength is currently 250
	static char tmpstr[251];

	zval *doc;
	if (Z_TYPE_P(cookie->retval) == IS_ARRAY && key != NULL) {
		MAKE_STD_ZVAL(doc);
		ZVAL_NULL(doc);

		memcpy(tmpstr, key, key_len);
		tmpstr[key_len] = '\0';
		add_assoc_zval_ex(cookie->retval, tmpstr, key_len+1, doc);
	} else {
		doc = cookie->retval;
	}
	return doc;
}

void cookie_error(const op_cookie *cookie, bucket_object *data, zval *doc,
				  lcb_error_t error TSRMLS_DC) {
	if (Z_TYPE_P(cookie->retval) == IS_ARRAY) {
		zval *zerror = create_lcb_exception(error TSRMLS_CC);
		object_init_ex(doc, metadoc_ce);
		zend_update_property(metadoc_ce, doc, "error", sizeof("error")-1, zerror TSRMLS_CC);
	} else {
		data->error = create_lcb_exception(error TSRMLS_CC);
	}
}

static void get_callback(lcb_t instance, const void *cookie, lcb_error_t error,
		const lcb_get_resp_t *resp)
{
	bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
	zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
	TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
    	if (make_metadoc_bytes(data, doc, resp->v.v0.bytes, resp->v.v0.nbytes,
    			resp->v.v0.cas, resp->v.v0.flags, resp->v.v0.datatype TSRMLS_CC) == FAILURE) {
    		cookie_error(cookie, data, doc, LCB_ERROR TSRMLS_CC);
    		return;
    	}
    } else {
    	cookie_error(cookie, data, doc, error TSRMLS_CC);
    }
}

static void store_callback(lcb_t instance, const void *cookie,
		lcb_storage_t operation, lcb_error_t error,
		const lcb_store_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
    	if (make_metadoc_bytes(data, doc, NULL, 0, resp->v.v0.cas, 0, 0 TSRMLS_CC) == FAILURE) {
    		cookie_error(cookie, data, doc, LCB_ERROR TSRMLS_CC);
    		return;
    	}
    } else {
    	cookie_error(cookie, data, doc, error TSRMLS_CC);
    }
}

static void arithmetic_callback(lcb_t instance, const void *cookie,
		lcb_error_t error, const lcb_arithmetic_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
    	if (make_metadoc_long(doc, resp->v.v0.value, resp->v.v0.cas, 0, 0 TSRMLS_CC) == FAILURE) {
    		cookie_error(cookie, data, doc, LCB_ERROR TSRMLS_CC);
    		return;
    	}
    } else {
    	cookie_error(cookie, data, doc, error TSRMLS_CC);
    }
}

static void remove_callback(lcb_t instance, const void *cookie,
		lcb_error_t error, const lcb_remove_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
    	if (make_metadoc(doc, NULL, resp->v.v0.cas, 0, 0 TSRMLS_CC) == FAILURE) {
    		cookie_error(cookie, data, doc, LCB_ERROR TSRMLS_CC);
    		return;
    	}
    } else {
    	cookie_error(cookie, data, doc, error TSRMLS_CC);
    }
}

static void touch_callback(lcb_t instance, const void *cookie,
        lcb_error_t error, const lcb_touch_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
    	if (make_metadoc(doc, NULL, resp->v.v0.cas, 0, 0 TSRMLS_CC) == FAILURE) {
    		cookie_error(cookie, data, doc, LCB_ERROR TSRMLS_CC);
    		return;
    	}
    } else {
    	cookie_error(cookie, data, doc, error TSRMLS_CC);
    }
}

static void flush_callback(lcb_t instance, const void *cookie,
			lcb_error_t error, const lcb_flush_resp_t *resp) {
	// Nothing to care about...
}

static void http_complete_callback(lcb_http_request_t request, lcb_t instance,
			const void *cookie, lcb_error_t error,
			const lcb_http_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, NULL, 0);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
		ZVAL_STRINGL(doc, resp->v.v0.bytes, resp->v.v0.nbytes, 1);
    } else {
    	cookie_error(cookie, data, NULL, error TSRMLS_CC);
    }
}

static void durability_callback(lcb_t instance, const void *cookie,
			lcb_error_t error, const lcb_durability_resp_t *resp) {
    bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
    zval *doc = cookie_get_doc(cookie, resp->v.v0.key, resp->v.v0.nkey);
    TSRMLS_FETCH();

    if (error == LCB_SUCCESS) {
		ZVAL_TRUE(doc);
    } else {
    	cookie_error(cookie, data, NULL, error TSRMLS_CC);
    }
}

static int pcbc_wait(lcb_t instance TSRMLS_DC)
{
	bucket_object *data = (bucket_object*)lcb_get_cookie(instance);
	data->error = NULL;

	lcb_wait(instance);

	if (data->error) {
		zend_throw_exception_object(data->error TSRMLS_CC);
		data->error = NULL;
		return 0;
	}

	return 1;
}





zend_class_entry *bucket_ce;

PHP_METHOD(Bucket, __construct)
{
	bucket_object *data = PHP_THISOBJ();
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
	char *connkey = NULL;
	bucket_connection *conn_iter;
	
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

	if (hosts && name) {
	    spprintf(&connkey, 512, "%s|%s", hosts, name);
	} else if(hosts) {
	    spprintf(&connkey, 512, "%s|default", hosts);
	} else {
	    spprintf(&connkey, 512, "localhost:8091|default");
	}
	
	conn_iter = first_conn;
	while (conn_iter) {
	    if (strcmp(conn_iter->key, connkey) == 0) {
	        break;
	    }
	    conn_iter = conn_iter->next;
	}

	if (!conn_iter)
	{
        memset(&create_options, 0, sizeof(create_options));
        create_options.v.v1.host = hosts;
        create_options.v.v1.bucket = name;
        create_options.v.v1.passwd = password;

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
        lcb_set_error_callback(instance, error_callback);
        lcb_set_get_callback(instance, get_callback);
        lcb_set_store_callback(instance, store_callback);
        lcb_set_arithmetic_callback(instance, arithmetic_callback);
        lcb_set_remove_callback(instance, remove_callback);
        lcb_set_touch_callback(instance, touch_callback);
        lcb_set_flush_callback(instance, flush_callback);
        lcb_set_http_complete_callback(instance, http_complete_callback);
        lcb_set_durability_callback(instance, durability_callback);

        err = lcb_connect(instance);
        if (err != LCB_SUCCESS) {
            lcb_destroy(instance);
            zend_throw_exception_object(create_lcb_exception(err TSRMLS_CC) TSRMLS_CC);
            RETURN_NULL();
        }

        if (pcbc_wait(instance TSRMLS_CC)) {
            bucket_connection *conn = pemalloc(sizeof(bucket_connection) ,1);
            conn->key = pestrdup(connkey, 1);
            conn->lcb = instance;
            conn->next = NULL;
            data->conn = conn;

            if (last_conn) {
                last_conn->next = conn;
                last_conn = conn;
            } else {
                first_conn = conn;
                last_conn = conn;
            }
        }
	} else {
        if (hosts) efree(hosts);
        if (name) efree(name);
        if (password) efree(password);
        if (ccache) efree(ccache);

	    data->conn = conn_iter;
	}

	efree(connkey);
}



// insert($id, $doc {, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, insert)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	const lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zvalue, *zexpiry, *zflags, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id|value|expiry,flags,groupid",
				  &zid, &zvalue, &zexpiry, &zflags, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zexpiry, IS_LONG, "expiry must be an integer");
		PCBC_CHECK_ZVAL(zflags, IS_LONG, "flags must be an integer");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_ADD;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);

		pcbc_zval_to_bytes(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zexpiry) {
			cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
		}
		if (zflags) {
			cmd[ii].v.v0.flags = Z_LVAL_P(zflags);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_store(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	PCBC_FREE_CMDV0BYTES(cmd, num_cmds);
	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// upsert($id, $doc {, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, upsert)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	const lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zvalue, *zexpiry, *zflags, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id|value|expiry,flags,groupid",
				  &zid, &zvalue, &zexpiry, &zflags, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zexpiry, IS_LONG, "expiry must be an integer");
		PCBC_CHECK_ZVAL(zflags, IS_LONG, "flags must be an integer");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_SET;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);

		pcbc_zval_to_bytes(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zexpiry) {
			cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
		}
		if (zflags) {
			cmd[ii].v.v0.flags = Z_LVAL_P(zflags);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_store(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	PCBC_FREE_CMDV0BYTES(cmd, num_cmds);
	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// save($id, $doc {, $cas, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, save)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	const lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zvalue, *zcas, *zexpiry, *zflags, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id|value|cas,expiry,flags,groupid",
				  &zid, &zvalue, &zcas, &zexpiry, &zflags, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zcas, IS_RESOURCE, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL(zexpiry, IS_LONG, "expiry must be an integer");
		PCBC_CHECK_ZVAL(zflags, IS_LONG, "flags must be an integer");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_REPLACE;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);

		pcbc_zval_to_bytes(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zcas) {
			cmd[ii].v.v0.cas = read_cas(zcas TSRMLS_CC);
		}
		if (zexpiry) {
			cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
		}
		if (zflags) {
			cmd[ii].v.v0.flags = Z_LVAL_P(zflags);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_store(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	PCBC_FREE_CMDV0BYTES(cmd, num_cmds);
	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// remove($id {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, remove)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_remove_cmd_t *cmd = NULL;
	const lcb_remove_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	op_cookie *cookie;
	zval *zid, *zcas, *zgroupid;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id||cas,groupid",
				  &zid, &zcas, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_remove_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_remove_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_remove_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zcas, IS_RESOURCE, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);

		if (zcas) {
			cmd[ii].v.v0.cas = read_cas(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_remove(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// get($id {, $lock, $groupid}) : MetaDoc
PHP_METHOD(Bucket, get)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_get_cmd_t *cmd = NULL;
	const lcb_get_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zlock, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id||lock,groupid",
				  &zid, &zlock, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_get_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_get_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_get_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zlock, IS_LONG, "lock must be an integer");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);
		if (zlock) {
			cmd[ii].v.v0.lock = Z_LVAL_P(zlock);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_get(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// unlock($id {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, unlock)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_unlock_cmd_t *cmd = NULL;
	const lcb_unlock_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zcas, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id||cas,groupid",
				  &zid, &zcas, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_unlock_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_unlock_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_unlock_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zcas, IS_RESOURCE, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);
		if (zcas) {
			cmd[ii].v.v0.cas = read_cas(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_unlock(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
	efree(cmds);
	efree(cmd);
}

// counter($id, $delta {, $initial, $expiry}) : MetaDoc
PHP_METHOD(Bucket, counter)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_arithmetic_cmd_t *cmd = NULL;
	const lcb_arithmetic_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zdelta, *zinitial, *zexpiry, *zgroupid;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id|delta|initial,expiry,groupid",
				  &zid, &zdelta, &zinitial, &zexpiry, &zgroupid);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_arithmetic_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_arithmetic_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_arithmetic_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zdelta, IS_LONG, "delta must be an integer");
		PCBC_CHECK_ZVAL(zinitial, IS_LONG, "initial must be an integer");
		PCBC_CHECK_ZVAL(zexpiry, IS_LONG, "expiry must be an integer");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);
		cmd[ii].v.v0.delta = Z_LVAL_P(zdelta);
		if (zinitial) {
			cmd[ii].v.v0.initial = Z_LVAL_P(zinitial);
			cmd[ii].v.v0.create = 1;
		}
		if (zexpiry) {
			cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_arithmetic(data->conn->lcb, cookie, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
	efree(cmds);
	efree(cmd);
}

PHP_METHOD(Bucket, flush)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_flush_cmd_t cmd = { 0 };
	const lcb_flush_cmd_t *cmds = { &cmd };

	lcb_flush(data->conn->lcb, NULL, 1, &cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);
}

PHP_METHOD(Bucket, http_request)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_http_cmd_t cmd = { 0 };
	op_cookie *cookie;
	lcb_http_type_t type;
	lcb_http_method_t method;
	const char *contenttype;
	zval *ztype, *zmethod, *zpath, *zbody, *zcontenttype;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzzzz",
				&ztype, &zmethod, &zpath, &zbody, &zcontenttype) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_LVAL_P(ztype) == 1) {
		type = LCB_HTTP_TYPE_VIEW;
	} else if (Z_LVAL_P(ztype) == 2) {
		type = LCB_HTTP_TYPE_MANAGEMENT;
	} else {
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
		RETURN_NULL();
	}

	if (Z_LVAL_P(zcontenttype) == 1) {
		contenttype = "application/json";
	} else if (Z_LVAL_P(zcontenttype) == 2) {
		contenttype = "application/x-www-form-urlencoded";
	} else {
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

    MAKE_STD_COOKIE(cookie, return_value, 0);

	lcb_make_http_request(data->conn->lcb, cookie, type, &cmd, NULL);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
}


PHP_METHOD(Bucket, durability)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_durability_cmd_t *cmd = NULL;
	lcb_durability_opts_t opts = { 0 };
	const lcb_durability_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zid, *zcas, *zgroupid, *zpersist, *zreplica;
	op_cookie *cookie;

	pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id||cas,groupid,persist_to,replicate_to",
				  &zid, &zcas, &zgroupid, &zpersist, &zreplica);

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_arithmetic_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_arithmetic_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_arithmetic_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL(zid, IS_STRING, "id must be a string");
		PCBC_CHECK_ZVAL(zcas, IS_RESOURCE, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL(zgroupid, IS_STRING, "groupid must be a string");
		PCBC_CHECK_ZVAL(zpersist, IS_LONG, "persist_to must be an integer");
		PCBC_CHECK_ZVAL(zreplica, IS_LONG, "replicate_to must be an integer");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = Z_STRVAL_P(zid);
		cmd[ii].v.v0.nkey = Z_STRLEN_P(zid);
		if (zcas) {
			cmd[ii].v.v0.cas = read_cas(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		// These are written through each iteration, but only used once.
		if (zpersist) {
			opts.v.v0.persist_to = Z_LVAL_P(zpersist);
		}
		if (zreplica) {
			opts.v.v0.replicate_to = Z_LVAL_P(zreplica);
		}

		cmds[ii] = &cmd[ii];
	}

	MAKE_STD_COOKIE(cookie, return_value, pcbc_pp_ismapped(&pp_state));

	lcb_durability_poll(data->conn->lcb, cookie, &opts, num_cmds, cmds);
	pcbc_wait(data->conn->lcb TSRMLS_CC);

	efree(cookie);
	efree(cmds);
	efree(cmd);
}


PHP_METHOD(Bucket, setTranscoder)
{
    bucket_object *data = PHP_THISOBJ();
    zval *zencoder, *zdecoder;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &zencoder, &zdecoder) == FAILURE) {
        RETURN_NULL();
    }

    FREE_ZVAL(data->encoder);
    MAKE_STD_ZVAL(data->encoder);
    ZVAL_ZVAL(data->encoder, zencoder, 1, NULL);

    FREE_ZVAL(data->decoder);
    MAKE_STD_ZVAL(data->decoder);
    ZVAL_ZVAL(data->decoder, zdecoder, 1, NULL);

    RETURN_NULL();
}

PHP_METHOD(Bucket, setOption)
{
    bucket_object *data = PHP_THISOBJ();
    lcb_uint32_t val;
    int type;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &type, &val) == FAILURE) {
        RETURN_NULL();
    }

    lcb_cntl(data->conn->lcb, LCB_CNTL_SET, type, &val);

    RETURN_LONG(val);
}

PHP_METHOD(Bucket, getOption)
{
    bucket_object *data = PHP_THISOBJ();
    lcb_uint32_t val;
    int type;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type) == FAILURE) {
        RETURN_NULL();
    }

    lcb_cntl(data->conn->lcb, LCB_CNTL_GET, type, &val);

    RETURN_LONG(val);
}

zend_function_entry bucket_methods[] = {
    PHP_ME(Bucket,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)

    PHP_ME(Bucket,  insert,          NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  upsert,          NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  save,            NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  remove,          NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  get,             NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  counter,         NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  flush,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  unlock,          NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  http_request,    NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  durability,      NULL, ZEND_ACC_PUBLIC)

    PHP_ME(Bucket,  setTranscoder,   NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  setOption,       NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Bucket,  getOption,       NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void init_cas(INIT_FUNC_ARGS) {
	le_cas = zend_register_list_destructors_ex(NULL, NULL, PHP_CAS_RES_NAME, module_number);
}

static void init_metadoc(INIT_FUNC_ARGS) {
	zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "CouchbaseMetaDoc", metadoc_methods);
    metadoc_ce = zend_register_internal_class(&ce TSRMLS_CC);

    zend_declare_property_null(metadoc_ce, "error", strlen("error"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(metadoc_ce, "value", strlen("value"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(metadoc_ce, "flags", strlen("flags"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(metadoc_ce, "cas", strlen("cas"), ZEND_ACC_PUBLIC TSRMLS_CC);
}

static void init_bucket(INIT_FUNC_ARGS) {
	zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "_CouchbaseBucket", bucket_methods);
    ce.create_object = bucket_create_handler;
    bucket_ce = zend_register_internal_class(&ce TSRMLS_CC);

    memcpy(&bucket_handlers,
    		zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    bucket_handlers.clone_obj = NULL;
}

#define PCBC_REGISTER_CONST(c) \
	REGISTER_LONG_CONSTANT("COUCHBASE_"#c, c, CONST_CS | CONST_PERSISTENT)
#define PCBC_REGISTER_LCBCONST(c) \
    REGISTER_LONG_CONSTANT("COUCHBASE_"#c, LCB_##c, CONST_CS | CONST_PERSISTENT)

void couchbase_init_bucket(INIT_FUNC_ARGS) {
    init_cas(INIT_FUNC_ARGS_PASSTHRU);
    init_metadoc(INIT_FUNC_ARGS_PASSTHRU);
    init_bucket(INIT_FUNC_ARGS_PASSTHRU);

    PCBC_REGISTER_CONST(PERSISTTO_MASTER);
    PCBC_REGISTER_CONST(PERSISTTO_ONE);
    PCBC_REGISTER_CONST(PERSISTTO_TWO);
    PCBC_REGISTER_CONST(PERSISTTO_THREE);
    PCBC_REGISTER_CONST(REPLICATETO_ONE);
    PCBC_REGISTER_CONST(REPLICATETO_TWO);
    PCBC_REGISTER_CONST(REPLICATETO_THREE);

    PCBC_REGISTER_LCBCONST(CNTL_OP_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_VIEW_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_DURABILITY_INTERVAL);
    PCBC_REGISTER_LCBCONST(CNTL_DURABILITY_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_HTTP_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_CONFIGURATION_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_CONFDELAY_THRESH);
    PCBC_REGISTER_LCBCONST(CNTL_CONFIG_NODE_TIMEOUT);
    PCBC_REGISTER_LCBCONST(CNTL_HTCONFIG_IDLE_TIMEOUT);
}
