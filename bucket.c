#include "couchbase.h"
#include "ext/standard/php_var.h"
#include "exception.h"
#include "datainfo.h"
#include "paramparser.h"
#include "zap.h"
#include "bucket.h"
#include "cas.h"
#include "metadoc.h"
#include "transcoding.h"
#include "opcookie.h"

#define _PCBC_CHECK_ZVAL(v,t,m) \
	if (v && Z_TYPE_P(v) != t) { \
		throw_pcbc_exception(m, LCB_EINVAL); \
		RETURN_NULL(); \
	}
#define PCBC_CHECK_ZVAL_STRING(v, m) \
    _PCBC_CHECK_ZVAL(v, IS_STRING, m)
#define PCBC_CHECK_ZVAL_LONG(v, m) \
    _PCBC_CHECK_ZVAL(v, IS_LONG, m)
#define PCBC_CHECK_ZVAL_CAS(v, m) \
    _PCBC_CHECK_ZVAL(v, IS_RESOURCE, m)
#define PCBC_CHECK_ZVAL_BOOL(v, m) \
    if (v && zap_zval_is_bool(v)) { \
        throw_pcbc_exception(m, LCB_EINVAL); \
        RETURN_NULL(); \
    }

#define PHP_THISOBJ() zap_fetch_this(bucket_object)

zap_class_entry bucket_class;
zend_class_entry *bucket_ce;

zap_FREEOBJ_FUNC(bucket_free_storage)
{
    bucket_object *obj = zap_get_object(bucket_object, object);

    zapval_destroy(obj->encoder);
    zapval_destroy(obj->decoder);
    zapval_destroy(obj->prefix);

    zend_object_std_dtor(&obj->std TSRMLS_CC);
    zap_free_object_storage(obj);
}

zap_CREATEOBJ_FUNC(bucket_create_handler)
{
    bucket_object *obj = zap_alloc_object_storage(bucket_object, type);

    zend_object_std_init(&obj->std, type TSRMLS_CC);
    zap_object_properties_init(&obj->std, type);

    zapval_alloc_empty_string(obj->encoder);
    zapval_alloc_empty_string(obj->decoder);
    zapval_alloc_empty_string(obj->prefix);

    obj->conn = NULL;

    return zap_finalize_object(obj, &bucket_class);
}

static zval * bop_get_return_doc(zval *return_value, zapval *key, int is_mapped)
{
    zval *doc = return_value;
    if (is_mapped) {
        if (!zap_zval_is_array(return_value)) {
            array_init(return_value);
        }
        {
            char tmpstr[251];
            HashTable *htretval = Z_ARRVAL_P(return_value);
            uint key_len = zapval_strlen_p(key);
            zapval new_doc;
            zapval_alloc_null(new_doc);

            memcpy(tmpstr, zapval_strval_p(key), key_len);
            tmpstr[key_len] = '\0';

            doc = zap_hash_str_add(
                    htretval, tmpstr, key_len, zapval_zvalptr(new_doc));
        }
    }
    return doc;
}

typedef struct {
    opcookie_res header;
    zapval key;
    zapval bytes;
    zapval flags;
    zapval datatype;
    zapval cas;
} opcookie_get_res;

static void get_callback(lcb_t instance, const void *cookie, lcb_error_t error,
		const lcb_get_resp_t *resp)
{
    opcookie_get_res *result = ecalloc(1, sizeof(opcookie_get_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);
    zapval_alloc_stringl(
            result->bytes, resp->v.v0.bytes, resp->v.v0.nbytes);
    zapval_alloc_long(result->flags,resp->v.v0.flags);
    zapval_alloc_long(result->datatype, resp->v.v0.datatype);
    alloc_cas(result->cas, resp->v.v0.cas TSRMLS_CC);

    opcookie_push((opcookie*)cookie, &result->header);
}

static lcb_error_t proc_get_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie, int is_mapped TSRMLS_DC)
{
    opcookie_get_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // If we are not mapped, we need to throw any op errors
    if (is_mapped == 0) {
        err = opcookie_get_first_error(cookie);
    }

    if (err == LCB_SUCCESS) {
        FOREACH_OPCOOKIE_RES(opcookie_get_res, res, cookie) {
            zval *doc = bop_get_return_doc(
                    return_value, &res->key, is_mapped);

            if (res->header.err == LCB_SUCCESS) {
                zapval value;
                zapval_alloc_null(value);
                pcbc_decode_value(bucket,
                        &value, &res->bytes, &res->flags, &res->datatype TSRMLS_CC);

                make_metadoc(doc, &value, &res->flags, &res->cas TSRMLS_CC);
                zapval_destroy(value);
            } else {
                make_metadoc_error(doc, res->header.err TSRMLS_CC);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_get_res, res, cookie) {
        zapval_destroy(res->key);
        zapval_destroy(res->bytes);
        zapval_destroy(res->flags);
        zapval_destroy(res->datatype);
        zapval_destroy(res->cas);
    }

    return err;
}

typedef struct {
    opcookie_res header;
    zapval key;
} opcookie_unlock_res;

static void unlock_callback(lcb_t instance, const void *cookie,
        lcb_error_t error, const lcb_unlock_resp_t *resp)
{
    opcookie_unlock_res *result = ecalloc(1, sizeof(opcookie_unlock_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);

    opcookie_push((opcookie*)cookie, &result->header);
}

static lcb_error_t proc_unlock_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie, int is_mapped TSRMLS_DC)
{
    opcookie_unlock_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // If we are not mapped, we need to throw any op errors
    if (is_mapped == 0) {
        err = opcookie_get_first_error(cookie);
    }

    if (err == LCB_SUCCESS) {
        FOREACH_OPCOOKIE_RES(opcookie_unlock_res, res, cookie) {
            zval *doc = bop_get_return_doc(
                    return_value, &res->key, is_mapped);

            if (res->header.err == LCB_SUCCESS) {
                make_metadoc(doc, NULL, NULL, NULL TSRMLS_CC);
            } else {
                make_metadoc_error(doc, res->header.err TSRMLS_CC);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_unlock_res, res, cookie) {
        zapval_destroy(res->key);
    }

    return err;
}
typedef struct {
    opcookie_res header;
    zapval key;
    zapval cas;
} opcookie_store_res;

static void store_callback(lcb_t instance, const void *cookie,
		lcb_storage_t operation, lcb_error_t error,
		const lcb_store_resp_t *resp) {
    opcookie_store_res *result = ecalloc(1, sizeof(opcookie_store_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);
    alloc_cas(result->cas, resp->v.v0.cas TSRMLS_CC);

    opcookie_push((opcookie*)cookie, &result->header);
}

static void remove_callback(lcb_t instance, const void *cookie,
        lcb_error_t error, const lcb_remove_resp_t *resp) {
    opcookie_store_res *result = ecalloc(1, sizeof(opcookie_store_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);
    alloc_cas(result->cas, resp->v.v0.cas TSRMLS_CC);

    opcookie_push((opcookie*)cookie, &result->header);
}

static void touch_callback(lcb_t instance, const void *cookie,
        lcb_error_t error, const lcb_touch_resp_t *resp) {
    opcookie_store_res *result = ecalloc(1, sizeof(opcookie_store_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);
    alloc_cas(result->cas, resp->v.v0.cas TSRMLS_CC);

    opcookie_push((opcookie*)cookie, &result->header);
}

static lcb_error_t proc_store_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie, int is_mapped TSRMLS_DC)
{
    opcookie_store_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // If we are not mapped, we need to throw any op errors
    if (is_mapped == 0) {
        err = opcookie_get_first_error(cookie);
    }

    if (err == LCB_SUCCESS) {
        FOREACH_OPCOOKIE_RES(opcookie_store_res, res, cookie) {
            zval *doc = bop_get_return_doc(
                    return_value, &res->key, is_mapped);

            if (res->header.err == LCB_SUCCESS) {
                make_metadoc(doc, NULL, NULL, &res->cas TSRMLS_CC);
            } else {
                make_metadoc_error(doc, res->header.err TSRMLS_CC);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_store_res, res, cookie) {
        zapval_destroy(res->key);
        zapval_destroy(res->cas);
    }

    return err;
}
#define proc_remove_results proc_store_results
#define proc_touch_results proc_store_results

typedef struct {
    opcookie_res header;
    zapval key;
    zapval value;
    zapval cas;
} opcookie_arithmetic_res;

static void arithmetic_callback(lcb_t instance, const void *cookie,
		lcb_error_t error, const lcb_arithmetic_resp_t *resp) {
    opcookie_arithmetic_res *result = ecalloc(1, sizeof(opcookie_arithmetic_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);
    zapval_alloc_long(result->value, resp->v.v0.value);
    alloc_cas(result->cas, resp->v.v0.cas TSRMLS_CC);

    opcookie_push((opcookie*)cookie, &result->header);
}

static lcb_error_t proc_arithmetic_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie, int is_mapped TSRMLS_DC)
{
    opcookie_arithmetic_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // If we are not mapped, we need to throw any op errors
    if (is_mapped == 0) {
        err = opcookie_get_first_error(cookie);
    }

    if (err == LCB_SUCCESS) {
        FOREACH_OPCOOKIE_RES(opcookie_arithmetic_res, res, cookie) {
            zval *doc = bop_get_return_doc(
                    return_value, &res->key, is_mapped);

            if (res->header.err == LCB_SUCCESS) {
                make_metadoc(doc, &res->value, NULL, &res->cas TSRMLS_CC);
            } else {
                make_metadoc_error(doc, res->header.err TSRMLS_CC);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_arithmetic_res, res, cookie) {
        zapval_destroy(res->key);
        zapval_destroy(res->value);
        zapval_destroy(res->cas);
    }

    return err;
}

typedef struct {
    opcookie_res header;
    zapval key;
} opcookie_durability_res;

static void durability_callback(lcb_t instance, const void *cookie,
            lcb_error_t error, const lcb_durability_resp_t *resp) {
    opcookie_durability_res *result = ecalloc(1, sizeof(opcookie_durability_res));
    TSRMLS_FETCH();

    result->header.err = error;
    zapval_alloc_stringl(
            result->key, resp->v.v0.key, resp->v.v0.nkey);

    opcookie_push((opcookie*)cookie, &result->header);

}

static lcb_error_t proc_durability_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie, int is_mapped TSRMLS_DC)
{
    opcookie_durability_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // If we are not mapped, we need to throw any op errors
    if (is_mapped == 0) {
        err = opcookie_get_first_error(cookie);
    }

    if (err == LCB_SUCCESS) {
        FOREACH_OPCOOKIE_RES(opcookie_durability_res, res, cookie) {
            zval *doc = bop_get_return_doc(
                    return_value, &res->key, is_mapped);

            if (res->header.err == LCB_SUCCESS) {
                make_metadoc(doc, NULL, NULL, NULL TSRMLS_CC);
            } else {
                make_metadoc_error(doc, res->header.err TSRMLS_CC);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_durability_res, res, cookie) {
        zapval_destroy(res->key);
    }

    return err;
}

typedef struct {
    opcookie_res header;
    lcb_U16 rflags;
    zapval row;
} opcookie_n1qlrow_res;

static void n1qlrow_callback(lcb_t instance, int ignoreme,
        const lcb_RESPN1QL *resp)
{
    opcookie_n1qlrow_res *result = ecalloc(1, sizeof(opcookie_n1qlrow_res));
    TSRMLS_FETCH();

    result->header.err = resp->rc;
    result->rflags = resp->rflags;
    zapval_alloc_stringl(
            result->row, resp->row, resp->nrow);

    opcookie_push((opcookie*)resp->cookie, &result->header);
}

static lcb_error_t proc_n1qlrow_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie TSRMLS_DC)
{
    opcookie_n1qlrow_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // Any error should cause everything to fail... for now?
    err = opcookie_get_first_error(cookie);

    if (err == LCB_SUCCESS) {
        zval *results_array = NULL;
        zapval zResults;
        zapval_alloc_array(zResults);
        array_init(return_value);
        results_array = zap_hash_str_add(Z_ARRVAL_P(return_value), "results", 7,
            zapval_zvalptr(zResults));

        FOREACH_OPCOOKIE_RES(opcookie_n1qlrow_res, res, cookie) {
            if (res->rflags & LCB_RESP_F_FINAL) {
                zap_hash_str_add(Z_ARRVAL_P(return_value), "meta", 4,
                        zapval_zvalptr(res->row));
                zapval_addref(res->row);
            } else {
                zap_hash_next_index_insert(
                        Z_ARRVAL_P(results_array), zapval_zvalptr(res->row));
                zapval_addref(res->row);
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_n1qlrow_res, res, cookie) {
        zapval_destroy(res->row);
    }

    return err;
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

static lcb_error_t proc_http_results(bucket_object *bucket, zval *return_value,
        opcookie *cookie TSRMLS_DC)
{
    opcookie_http_res *res;
    lcb_error_t err = LCB_SUCCESS;

    // Any error should cause everything to fail... for now?
    err = opcookie_get_first_error(cookie);

    if (err == LCB_SUCCESS) {
        int has_value = 0;
        FOREACH_OPCOOKIE_RES(opcookie_http_res, res, cookie) {
            if (has_value == 0) {
                zap_zval_zval_p(return_value, zapval_zvalptr(res->bytes), 1, 0);
                has_value = 1;
            } else {
                err = LCB_ERROR;
                break;
            }
        }
    }

    FOREACH_OPCOOKIE_RES(opcookie_http_res, res, cookie) {
        zapval_destroy(res->bytes);
    }

    return err;
}

PHP_METHOD(Bucket, __construct)
{
	bucket_object *data = PHP_THISOBJ();
	zval *zdsn = NULL;
	zval *zname = NULL;
	zval *zpassword = NULL;
	char *dsn = NULL;
	char *name = NULL;
	char *password = NULL;
	lcb_error_t err;
	lcb_t instance;
	struct lcb_create_st create_options;
	char *connkey = NULL;
	pcbc_lcb *conn_iter, *conn;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz",
			&zdsn, &zname, &zpassword) == FAILURE) {
		RETURN_NULL();
	}

	if (zap_zval_is_undef(zdsn)) {
	    zdsn = NULL;
	}
    if (zap_zval_is_undef(zname)) {
        zname = NULL;
    }
    if (zap_zval_is_undef(zpassword)) {
        zpassword = NULL;
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

	spprintf(&connkey, 512, "%s|%s|%s",
			dsn ? dsn : "",
			name ? name : "",
			password ? password : "");

	conn_iter = PCBCG(first_bconn);
	while (conn_iter) {
		if (strcmp(conn_iter->key, connkey) == 0) {
			break;
		}
		conn_iter = conn_iter->next;
	}

	if (!conn_iter)
	{
		memset(&create_options, 0, sizeof(create_options));
		create_options.version = 3;
		create_options.v.v3.connstr = dsn;
		create_options.v.v3.username = name;
		create_options.v.v3.passwd = password;
		create_options.v.v3.type = LCB_TYPE_BUCKET;
		err = lcb_create(&instance, &create_options);

		if (dsn) efree(dsn);
		if (name) efree(name);
		if (password) efree(password);

		if (err != LCB_SUCCESS) {
			efree(connkey);
			throw_lcb_exception(err);
			RETURN_NULL();
		}

		lcb_set_get_callback(instance, get_callback);
		lcb_set_unlock_callback(instance, unlock_callback);
		lcb_set_store_callback(instance, store_callback);
		lcb_set_arithmetic_callback(instance, arithmetic_callback);
		lcb_set_remove_callback(instance, remove_callback);
		lcb_set_touch_callback(instance, touch_callback);
		lcb_set_http_complete_callback(instance, http_complete_callback);
		lcb_set_durability_callback(instance, durability_callback);

		err = lcb_connect(instance);
		if (err != LCB_SUCCESS) {
			efree(connkey);
			lcb_destroy(instance);
			throw_lcb_exception(err);
			RETURN_NULL();
		}

		// We use lcb_wait here as no callbacks are invoked by connect.
		lcb_wait(instance);

		err = lcb_get_bootstrap_status(instance);
		if (err != LCB_SUCCESS) {
			efree(connkey);
			lcb_destroy(instance);
			throw_lcb_exception(err);
			RETURN_NULL();
		}

		conn = pemalloc(sizeof(pcbc_lcb), 1);
		conn->key = pestrdup(connkey, 1);
		conn->lcb = instance;
		conn->next = NULL;
		data->conn = conn;

		if (PCBCG(last_bconn)) {
			PCBCG(last_bconn)->next = conn;
			PCBCG(last_bconn) = conn;
		} else {
			PCBCG(first_bconn) = conn;
			PCBCG(last_bconn) = conn;
		}
	} else {
		if (dsn) efree(dsn);
		if (name) efree(name);
		if (password) efree(password);

		data->conn = conn_iter;
	}

	efree(connkey);
}

// insert($id, $doc {, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, insert)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zvalue, *zexpiry, *zflags, *zgroupid;
	opcookie *cookie;
    lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id|value|expiry,flags,groupid",
				  &id, &zvalue, &zexpiry, &zflags, &zgroupid) != SUCCESS)
	{
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
		PCBC_CHECK_ZVAL_LONG(zflags, "flags must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_ADD;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		pcbc_encode_value(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
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

	cookie = opcookie_init();

	err = lcb_store(data->conn->lcb, cookie,
	        num_cmds, (const lcb_store_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_store_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    for (ii = 0; ii < num_cmds; ++ii) {
        efree((void*)cmds[ii]->v.v0.bytes);
    }
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// upsert($id, $doc {, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, upsert)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	zval *zvalue, *zexpiry, *zflags, *zgroupid;
	pcbc_pp_id id;
	opcookie *cookie;
    lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id|value|expiry,flags,groupid",
				  &id, &zvalue, &zexpiry, &zflags, &zgroupid) != SUCCESS)
	{
	    throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
	    RETURN_NULL();
	}

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
		PCBC_CHECK_ZVAL_LONG(zflags, "flags must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_SET;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		pcbc_encode_value(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
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

	cookie = opcookie_init();

	err = lcb_store(data->conn->lcb, cookie,
	        num_cmds, (const lcb_store_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_store_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    for (ii = 0; ii < num_cmds; ++ii) {
        efree((void*)cmds[ii]->v.v0.bytes);
    }
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// save($id, $doc {, $cas, $expiry, $groupid}) : MetaDoc
PHP_METHOD(Bucket, replace)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zvalue, *zcas, *zexpiry, *zflags, *zgroupid;
	opcookie *cookie;
    lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id|value|cas,expiry,flags,groupid",
				  &id, &zvalue, &zcas, &zexpiry, &zflags, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
		PCBC_CHECK_ZVAL_LONG(zflags, "flags must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_REPLACE;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		pcbc_encode_value(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zcas) {
			cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
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

	cookie = opcookie_init();

	err = lcb_store(data->conn->lcb, cookie,
	        num_cmds, (const lcb_store_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_store_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    for (ii = 0; ii < num_cmds; ++ii) {
        efree((void*)cmds[ii]->v.v0.bytes);
    }
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// append($id, $doc {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, append)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zvalue, *zcas, *zgroupid;
	opcookie *cookie;
    lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id|value|cas,groupid",
				  &id, &zvalue, &zcas, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_APPEND;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		pcbc_encode_value(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zcas) {
			cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		// Flags ignored for this op, enforced by libcouchbase
        cmd[ii].v.v0.flags = 0;

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_store(data->conn->lcb, cookie,
	        num_cmds, (const lcb_store_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_store_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    for (ii = 0; ii < num_cmds; ++ii) {
        efree((void*)cmds[ii]->v.v0.bytes);
    }
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// append($id, $doc {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, prepend)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_store_cmd_t *cmd = NULL;
	lcb_store_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zvalue, *zcas, *zgroupid;
	opcookie *cookie;
	lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id|value|cas,groupid",
				  &id, &zvalue, &zcas, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_store_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_store_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_store_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.operation = LCB_PREPEND;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		pcbc_encode_value(data, zvalue, &cmd[ii].v.v0.bytes, &cmd[ii].v.v0.nbytes,
				&cmd[ii].v.v0.flags, &cmd[ii].v.v0.datatype TSRMLS_CC);

		if (zcas) {
			cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

        // Flags ignored for this op, enforced by libcouchbase
        cmd[ii].v.v0.flags = 0;

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_store(data->conn->lcb, cookie,
	        num_cmds, (const lcb_store_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_store_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    for (ii = 0; ii < num_cmds; ++ii) {
        efree((void*)cmds[ii]->v.v0.bytes);
    }
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// remove($id {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, remove)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_remove_cmd_t *cmd = NULL;
	lcb_remove_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	opcookie *cookie;
	zval *zcas, *zgroupid;
	lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state, "id||cas,groupid",
				  &id, &zcas, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_remove_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_remove_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_remove_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;

		if (zcas) {
			cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_remove(data->conn->lcb, cookie,
	        num_cmds, (const lcb_remove_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_remove_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// touch($id {, $lock, $groupid}) : MetaDoc
PHP_METHOD(Bucket, touch)
{
    bucket_object *data = PHP_THISOBJ();
    lcb_touch_cmd_t *cmd = NULL;
    lcb_touch_cmd_t **cmds = NULL;
    int ii, num_cmds;
    pcbc_pp_state pp_state;
    pcbc_pp_id id;
    zval *zexpiry, *zgroupid;
    opcookie *cookie;
    lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
    if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
                  "id|expiry|groupid",
                  &id, &zexpiry, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

    num_cmds = pcbc_pp_keycount(&pp_state);
    cmd = emalloc(sizeof(lcb_touch_cmd_t) * num_cmds);
    cmds = emalloc(sizeof(lcb_touch_cmd_t*) * num_cmds);
    memset(cmd, 0, sizeof(lcb_touch_cmd_t) * num_cmds);

    for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
        PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
        PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

        cmd[ii].version = 0;
        cmd[ii].v.v0.key = id.str;
        cmd[ii].v.v0.nkey = id.len;
        cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
        if (zgroupid) {
            cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
            cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
        }

        cmds[ii] = &cmd[ii];
    }

    cookie = opcookie_init();

    err = lcb_touch(data->conn->lcb, cookie,
            num_cmds, (const lcb_touch_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_touch_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// get($id {, $lock, $groupid}) : MetaDoc
PHP_METHOD(Bucket, get)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_get_cmd_t *cmd = NULL;
	lcb_get_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zlock, *zexpiry, *zgroupid;
	opcookie *cookie;
	lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
				  "id||lockTime,expiry,groupid",
				  &id, &zlock, &zexpiry, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_get_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_get_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_get_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_LONG(zlock, "lock must be an integer");
		PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;
		if (zexpiry) {
			cmd[ii].v.v0.lock = 0;
			cmd[ii].v.v0.exptime = Z_LVAL_P(zexpiry);
		} else if (zlock) {
			cmd[ii].v.v0.lock = 1;
			cmd[ii].v.v0.exptime = Z_LVAL_P(zlock);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_get(data->conn->lcb, cookie,
	        num_cmds, (const lcb_get_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_get_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
	efree(cmds);
	efree(cmd);

	if (err != LCB_SUCCESS) {
	    throw_lcb_exception(err);
	}
}

// get($id {, $lock, $groupid}) : MetaDoc
PHP_METHOD(Bucket, getFromReplica)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_get_replica_cmd_t *cmd = NULL;
	lcb_get_replica_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zindex, *zgroupid;
	opcookie *cookie;
	lcb_error_t err;

	// Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
				  "id||index,groupid",
				  &id, &zindex, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_get_replica_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_get_replica_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_get_replica_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_LONG(zindex, "index must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 1;
		cmd[ii].v.v1.key = id.str;
		cmd[ii].v.v1.nkey = id.len;
		if (zindex) {
			cmd[ii].v.v1.index = Z_LVAL_P(zindex);
			if (cmd[ii].v.v1.index >= 0) {
				cmd[ii].v.v1.strategy = LCB_REPLICA_SELECT;
			} else {
				cmd[ii].v.v1.strategy = LCB_REPLICA_FIRST;
			}
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_get_replica(data->conn->lcb, cookie,
	        num_cmds, (const lcb_get_replica_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_get_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// unlock($id {, $cas, $groupid}) : MetaDoc
PHP_METHOD(Bucket, unlock)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_unlock_cmd_t *cmd = NULL;
	lcb_unlock_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zcas, *zgroupid;
	opcookie *cookie;
	lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id||cas,groupid",
				  &id, &zcas, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_unlock_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_unlock_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_unlock_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;
		if (zcas) {
			cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
		}
		if (zgroupid) {
			cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
			cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
		}

		cmds[ii] = &cmd[ii];
	}

	cookie = opcookie_init();

	err = lcb_unlock(data->conn->lcb, cookie,
	        num_cmds, (const lcb_unlock_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_unlock_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

// counter($id, $delta {, $initial, $expiry}) : MetaDoc
PHP_METHOD(Bucket, counter)
{
	bucket_object *data = PHP_THISOBJ();
	lcb_arithmetic_cmd_t *cmd = NULL;
	lcb_arithmetic_cmd_t **cmds = NULL;
	int ii, num_cmds;
	pcbc_pp_state pp_state;
	pcbc_pp_id id;
	zval *zdelta, *zinitial, *zexpiry, *zgroupid;
	opcookie *cookie;
	lcb_error_t err;

  // Note that groupid is experimental here and should not be used.
	if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
	              "id|delta|initial,expiry,groupid",
				  &id, &zdelta, &zinitial, &zexpiry, &zgroupid) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

	num_cmds = pcbc_pp_keycount(&pp_state);
	cmd = emalloc(sizeof(lcb_arithmetic_cmd_t) * num_cmds);
	cmds = emalloc(sizeof(lcb_arithmetic_cmd_t*) * num_cmds);
	memset(cmd, 0, sizeof(lcb_arithmetic_cmd_t) * num_cmds);

	for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
		PCBC_CHECK_ZVAL_LONG(zdelta, "delta must be an integer");
		PCBC_CHECK_ZVAL_LONG(zinitial, "initial must be an integer");
		PCBC_CHECK_ZVAL_LONG(zexpiry, "expiry must be an integer");
		PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");

		cmd[ii].version = 0;
		cmd[ii].v.v0.key = id.str;
		cmd[ii].v.v0.nkey = id.len;
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

	cookie = opcookie_init();

	err = lcb_arithmetic(data->conn->lcb, cookie,
	        num_cmds, (const lcb_arithmetic_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_arithmetic_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

PHP_METHOD(Bucket, durability)
{
    bucket_object *data = PHP_THISOBJ();
    lcb_durability_cmd_t *cmd = NULL;
    lcb_durability_opts_t opts = { 0 };
    lcb_durability_cmd_t **cmds = NULL;
    int ii, num_cmds;
    pcbc_pp_state pp_state;
    pcbc_pp_id id;
    zval *zcas, *zgroupid, *zpersist, *zreplica;
    opcookie *cookie;
    lcb_error_t err;

    if (pcbc_pp_begin(ZEND_NUM_ARGS() TSRMLS_CC, &pp_state,
                  "id||cas,groupid,persist_to,replicate_to",
                  &id, &zcas, &zgroupid, &zpersist, &zreplica) != SUCCESS)
    {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

    num_cmds = pcbc_pp_keycount(&pp_state);
    cmd = emalloc(sizeof(lcb_durability_cmd_t) * num_cmds);
    cmds = emalloc(sizeof(lcb_durability_cmd_t*) * num_cmds);
    memset(cmd, 0, sizeof(lcb_durability_cmd_t) * num_cmds);

    for (ii = 0; pcbc_pp_next(&pp_state); ++ii) {
        PCBC_CHECK_ZVAL_CAS(zcas, "cas must be a CAS resource");
        PCBC_CHECK_ZVAL_STRING(zgroupid, "groupid must be a string");
        PCBC_CHECK_ZVAL_LONG(zpersist, "persist_to must be an integer");
        PCBC_CHECK_ZVAL_LONG(zreplica, "replicate_to must be an integer");

        cmd[ii].version = 0;
        cmd[ii].v.v0.key = id.str;
        cmd[ii].v.v0.nkey = id.len;
        if (zcas) {
            cmd[ii].v.v0.cas = cas_retrieve(zcas TSRMLS_CC);
        }
        if (zgroupid) {
            cmd[ii].v.v0.hashkey = Z_STRVAL_P(zgroupid);
            cmd[ii].v.v0.nhashkey = Z_STRLEN_P(zgroupid);
        }

        // These are written through each iteration, but only used once.
        if (zpersist) {
            opts.v.v0.persist_to = (lcb_U16)Z_LVAL_P(zpersist);
        }
        if (zreplica) {
            opts.v.v0.replicate_to = (lcb_U16)Z_LVAL_P(zreplica);
        }

        cmds[ii] = &cmd[ii];
    }

    cookie = opcookie_init();

    err = lcb_durability_poll(data->conn->lcb, cookie, &opts,
            num_cmds, (const lcb_durability_cmd_t*const*)cmds);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_durability_results(data, return_value,
                cookie, pcbc_pp_ismapped(&pp_state) TSRMLS_CC);
    }

    opcookie_destroy(cookie);
    efree(cmds);
    efree(cmd);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

PHP_METHOD(Bucket, n1ql_request)
{
    bucket_object *data = PHP_THISOBJ();
    lcb_CMDN1QL cmd = { 0 };
    opcookie *cookie;
    zval *zbody, *zadhoc;
    zapval zResults;
    lcb_error_t err;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz",
                &zbody, &zadhoc) == FAILURE) {
        throw_pcbc_exception("Invalid arguments.", LCB_EINVAL);
        RETURN_NULL();
    }

    PCBC_CHECK_ZVAL_STRING(zbody, "body must be a string");
    PCBC_CHECK_ZVAL_BOOL(zadhoc, "adhoc must be a bool");

    cmd.callback = n1qlrow_callback;
    cmd.content_type = "application/json";
    cmd.query = Z_STRVAL_P(zbody);
    cmd.nquery = Z_STRLEN_P(zbody);

    if (zap_zval_boolval(zadhoc) == 0) {
        cmd.cmdflags |= LCB_CMDN1QL_F_PREPCACHE;
    }

    cookie = opcookie_init();

    // Execute query
    err = lcb_n1ql_query(data->conn->lcb, cookie, &cmd);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_n1qlrow_results(data, return_value, cookie TSRMLS_CC);
    }

    opcookie_destroy(cookie);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}

PHP_METHOD(Bucket, http_request)
{
	bucket_object *data = PHP_THISOBJ();
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
	} else if (Z_LVAL_P(ztype) == 3) {
	    type = LCB_HTTP_TYPE_N1QL;
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

	if (Z_TYPE_P(zpath) == IS_STRING) {
	    cmd.v.v0.path = Z_STRVAL_P(zpath);
	    cmd.v.v0.npath = Z_STRLEN_P(zpath);
	}
	if (Z_TYPE_P(zbody) == IS_STRING) {
		cmd.v.v0.body = Z_STRVAL_P(zbody);
		cmd.v.v0.nbody = Z_STRLEN_P(zbody);
	}
	cmd.v.v0.method = method;
	cmd.v.v0.chunked = 0;
	cmd.v.v0.content_type = contenttype;

	cookie = opcookie_init();

	err = lcb_make_http_request(data->conn->lcb, cookie, type, &cmd, NULL);

    if (err == LCB_SUCCESS) {
        lcb_wait(data->conn->lcb);

        err = proc_http_results(data, return_value, cookie TSRMLS_CC);
    }

    opcookie_destroy(cookie);

    if (err != LCB_SUCCESS) {
        throw_lcb_exception(err);
    }
}


PHP_METHOD(Bucket, setTranscoder)
{
	bucket_object *data = PHP_THISOBJ();
	zval *zencoder, *zdecoder;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &zencoder, &zdecoder) == FAILURE) {
		RETURN_NULL();
	}

	zapval_destroy(data->encoder);
	zapval_alloc_zval(data->encoder, zencoder, 1, 0);

	zapval_destroy(data->decoder);
	zapval_alloc_zval(data->decoder, zdecoder, 1, 0);

	RETURN_NULL();
}

PHP_METHOD(Bucket, setOption)
{
	bucket_object *data = PHP_THISOBJ();
	long type, val;
	lcb_uint32_t lcbval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &type, &val) == FAILURE) {
		RETURN_NULL();
	}

	lcbval = val;
	lcb_cntl(data->conn->lcb, LCB_CNTL_SET, type, &lcbval);

	RETURN_LONG(val);
}

PHP_METHOD(Bucket, getOption)
{
	bucket_object *data = PHP_THISOBJ();
	long type;
	lcb_uint32_t lcbval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type) == FAILURE) {
		RETURN_NULL();
	}

	lcb_cntl(data->conn->lcb, LCB_CNTL_GET, type, &lcbval);

	RETURN_LONG(lcbval);
}

zend_function_entry bucket_methods[] = {
	PHP_ME(Bucket,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)

	PHP_ME(Bucket,  insert,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  upsert,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  replace,         NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  append,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  prepend,         NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  remove,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  get,             NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  getFromReplica,  NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  touch,           NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  counter,         NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  unlock,          NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  n1ql_request,    NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  http_request,    NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  durability,      NULL, ZEND_ACC_PUBLIC)

	PHP_ME(Bucket,  setTranscoder,   NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  setOption,       NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Bucket,  getOption,       NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

void couchbase_init_bucket(INIT_FUNC_ARGS) {
    zap_init_class_entry(&bucket_class, "_CouchbaseBucket",
            bucket_methods);
    bucket_class.create_obj = bucket_create_handler;
    bucket_class.free_obj = bucket_free_storage;
    bucket_ce = zap_register_internal_class(&bucket_class, bucket_object);
}

void couchbase_shutdown_bucket(SHUTDOWN_FUNC_ARGS) {
	pcbc_lcb *cur, *next;
	next = PCBCG(first_bconn);
	while (next) {
		cur = next;
		next = cur->next;
		lcb_destroy(cur->lcb);
		free(cur->key);
		free(cur);
	}
	PCBCG(first_bconn) = NULL;
	PCBCG(last_bconn) = NULL;
}
