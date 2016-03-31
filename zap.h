/**
 * Zend Abstractions for Php
 */

#ifndef ZAP_H_
#define ZAP_H_

#include <php.h>

typedef struct _zap_class_entry {
#if PHP_VERSION_ID >= 70000
    zend_object* (*create_obj)(zend_class_entry *class_type);
    zend_object_free_obj_t free_obj;
    zend_object_dtor_obj_t dtor_obj;
    zend_object_clone_obj_t clone_obj;
#else
    zend_object_value (*create_obj)(zend_class_entry *type TSRMLS_DC);
    zend_objects_free_object_storage_t free_obj;
    zend_objects_store_dtor_t dtor_obj;
    zend_objects_store_clone_t clone_obj;
#endif

    struct {
        zend_class_entry ce;
        zend_object_handlers handlers;
    } _internal;
} zap_class_entry;

#define zap_init_class_entry(cls, name, methods) \
    memset((cls), 0, sizeof(zap_class_entry));\
    INIT_CLASS_ENTRY((cls)->_internal.ce, name, methods)

#if PHP_VERSION_ID >= 70000
static zend_class_entry * _zap_register_internal_class(zap_class_entry *cls, int offset TSRMLS_DC) {
    zend_class_entry *ce;

    memcpy(&cls->_internal.handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    cls->_internal.handlers.offset = offset;

    if (cls->free_obj != NULL) {
        cls->_internal.handlers.free_obj = cls->free_obj;
    }
    if (cls->dtor_obj != NULL) {
        cls->_internal.handlers.dtor_obj = cls->dtor_obj;
    }
    if (cls->clone_obj != NULL) {
        cls->_internal.handlers.clone_obj = cls->clone_obj;
    }

    cls->_internal.ce.create_object = cls->create_obj;
    ce = zend_register_internal_class(&cls->_internal.ce TSRMLS_CC);
    return ce;
}
#define zap_register_internal_class(cls, Type) \
    _zap_register_internal_class(cls, XtOffsetOf(struct Type, std) TSRMLS_CC)

static inline zend_object * _zap_finalize_object(zend_object *std, zap_class_entry* cls) {
    std->handlers = &(cls)->_internal.handlers;
    return std;
}
#define zap_finalize_object(obj, cls) \
    _zap_finalize_object(&obj->std, cls)

#else

static zend_class_entry * _zap_register_internal_class(zap_class_entry *cls TSRMLS_DC) {
    zend_class_entry *ce;

    memcpy(&cls->_internal.handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));

    cls->_internal.ce.create_object = cls->create_obj;
    ce = zend_register_internal_class(&cls->_internal.ce TSRMLS_CC);
    return ce;
}
#define zap_register_internal_class(cls, Type) \
    _zap_register_internal_class(cls TSRMLS_CC)

static inline zend_object_value _zap_finalize_object(void *obj, zap_class_entry* cls TSRMLS_DC) {
    zend_object_value retval;
    retval.handle = zend_objects_store_put(
                obj, cls->dtor_obj, cls->free_obj, cls->clone_obj TSRMLS_CC);
    retval.handlers = &(cls)->_internal.handlers;
    return retval;
}
#define zap_finalize_object(obj, cls) \
    _zap_finalize_object(obj, cls TSRMLS_CC)

#endif


#if PHP_VERSION_ID >= 70000

#define zap_DTORRES_FUNC(name) \
    void name(zend_resource *rsrc TSRMLS_DC)

#define zap_fetch_resource(z, name, le) \
    zend_fetch_resource_ex(z, name, le)

#else

#define zap_DTORRES_FUNC(name) \
    void name(zend_rsrc_list_entry *rsrc TSRMLS_DC)

#define zap_fetch_resource(z, name, le) \
    zend_fetch_resource(&z TSRMLS_CC, -1, name, NULL, 1, le)

#endif

#if PHP_VERSION_ID >= 70000

#define zap_ZEND_OBJECT_START
#define zap_ZEND_OBJECT_END zend_object std;

#define zap_get_object(Type, object) ((struct Type *)((char*)(object) - XtOffsetOf(struct Type, std)))
#define zap_fetch_this(Type) zap_get_object(Type, Z_OBJ_P(getThis()))

#define zap_CREATEOBJ_FUNC(name) \
    zend_object * name(zend_class_entry *type TSRMLS_DC)
#define zap_FREEOBJ_FUNC(name) \
    void name(zend_object *object TSRMLS_DC)

#define zap_alloc_object_storage(Type, type) \
    ((Type *)ecalloc(1, sizeof(Type) + zend_object_properties_size(type)))
#define zap_free_object_storage(o)

#else

#define zap_ZEND_OBJECT_START zend_object std;
#define zap_ZEND_OBJECT_END

#define zap_get_object(Type, object) ((Type*)object)
#define zap_fetch_this(Type) ((Type*)zend_object_store_get_object(getThis() TSRMLS_CC))

#define zap_CREATEOBJ_FUNC(name) \
    zend_object_value name(zend_class_entry *type TSRMLS_DC)
#define zap_FREEOBJ_FUNC(name) \
    void name(void *object TSRMLS_DC)

#define zap_alloc_object_storage(Type, type) \
    ((Type *)ecalloc(1, sizeof(Type)))
#define zap_free_object_storage(o) efree(o)

#endif

#if PHP_VERSION_ID >= 70000
typedef zval zapval;

#define zapval_zvalptr(v) (&v)
#define zapval_zvalptr_p(v) (v)

#define zapvalptr_from_zvalptr(v) (v)
#define zapval_from_zvalptr(v) (*v)

// These all set a zval* to something
#define zap_zval_undef_p(v) \
    ZVAL_UNDEF(v)
#define zap_zval_null_p(v) \
    ZVAL_NULL(v)
#define zap_zval_bool_p(v, b) \
    if (b) { ZVAL_TRUE(v); } else { ZVAL_FALSE(v); }
#define zap_zval_empty_string_p(v) \
    ZVAL_EMPTY_STRING(v)
#define zap_zval_stringl_p(v, b, nb) \
    if (b != NULL || nb > 0) { ZVAL_STRINGL(v, b, nb); } \
    else { ZVAL_EMPTY_STRING(v); }
#define zap_zval_long_p(v, n) \
    ZVAL_LONG(v, n)
#define zap_zval_array_p(v) \
    array_init(v)
#define zap_zval_zval_p(v, z, copy, dtor) \
    ZVAL_ZVAL(v, z, copy, dtor)
#define zap_zval_res_p(v, ptr, type) \
    ZVAL_RES(v, zend_register_resource(ptr, type))

#define zap_zval_is_undef(v) \
    (Z_TYPE_P(v) == IS_UNDEF)
#define zap_zval_is_bool(v) \
    (Z_TYPE_P(v) == IS_TRUE || Z_TYPE_P(v) == IS_FALSE)
#define zap_zval_is_array(v) \
    (Z_TYPE_P(v) == IS_ARRAY)

#define zap_zval_boolval(v)  \
    (Z_TYPE_P(v) == IS_TRUE ? 1 : 0)

// These all set a zapval to something
#define zapval_undef(v) \
    zap_zval_undef_p(&v)
#define zapval_null(v) \
    zap_zval_null_p(&v)
#define zapval_empty_string(v) \
    zap_zval_empty_string_p(&v)
#define zapval_stringl(v, b, nb) \
    zap_zval_stringl_p(&v, b, nb)
#define zapval_long(v, n) \
    zap_zval_long_p(&v, n)
#define zapval_array(v) \
    zap_zval_array_p(&v)
#define zapval_zval(v, z, copy, dtor) \
    zap_zval_zval_p(&v, z, copy, dtor)
#define zapval_res(v, ptr, type) \
    zap_zval_res_p(&v, ptr, type)

// These all allocate and set a zapval to something
#define zapval_alloc(v) \
    zapval_undef(v)
#define zapval_addref(v) \
    Z_TRY_ADDREF(v)
#define zapval_destroy(v) \
    zval_ptr_dtor(&v)

#define zapval_is_undef(v) (Z_TYPE(v) == IS_UNDEF)
#define zapval_is_bool(v) (Z_TYPE(v) == IS_TRUE || Z_TYPE(v) == IS_FALSE)
#define zapval_is_string(v) (Z_TYPE(v) == IS_STRING)
#define zapval_is_long(v) (Z_TYPE(v) == IS_LONG)
#define zapval_is_array(v) (Z_TYPE(v) == IS_ARRAY)

#define zapval_arrval(v) Z_ARRVAL(v)

#define zapval_strlen_p(v) Z_STRLEN_P(v)
#define zapval_strval_p(v) Z_STRVAL_P(v)
#define zapval_lval_p(v) Z_LVAL_P(v)

#define zap_throw_exception_object(o) zend_throw_exception_object(&o TSRMLS_CC)

// int param_count, zapval *args
#define zap_get_parameters_array_ex(param_count, args) \
    _zend_get_parameters_array_ex(param_count, args TSRMLS_CC)

#define zap_call_user_function(o, f, r, np, p) \
    call_user_function(CG(function_table), o, &f, r, np, p TSRMLS_CC)

#define zap_hash_str_add(ht, k, nk, hv) \
    zend_hash_str_add(ht, k, nk, hv)
#define zap_hash_next_index_insert(ht, hv) \
    zend_hash_next_index_insert(ht, hv)
#define zap_hash_str_find(ht, k, nk) \
    zend_hash_str_find(ht, k, nk)
#define zap_hash_index_find(ht, i) \
    zend_hash_index_find(ht, i)
#define zap_hash_get_current_data_ex(ht, pos) \
    zend_hash_get_current_data_ex(ht, pos)

static inline int zap_hash_str_get_current_key_ex(HashTable *ht, char **str,
        uint *len, zend_ulong *num_index, HashPosition *pos) {
    zend_string *zstr = NULL;
    int key_type = zend_hash_get_current_key_ex(ht, &zstr, num_index, pos);
    if (zstr != NULL) {
        *str = zstr->val;
        *len = zstr->len;
    } else {
        *str = NULL;
        *len = 0;
    }
    return key_type;
}

#else
typedef zval* zapval;

#define zapval_zvalptr(v) (v)
#define zapval_zvalptr_p(v) (*v)

#define zapvalptr_from_zvalptr(v) (&v)
#define zapval_from_zvalptr(v) (v)

// TODO: v=NULL will leak memory!

// These all set a zval* to something
#define zap_zval_undef_p(v) \
    v = NULL;
#define zap_zval_null_p(v) \
    ZVAL_NULL(v)
#define zap_zval_bool_p(v, b) \
    if (b) { ZVAL_TRUE(v); } else { ZVAL_FALSE(v); }
#define zap_zval_empty_string_p(v) \
    ZVAL_EMPTY_STRING(v)
#define zap_zval_stringl_p(v, b, nb) \
    if (b != NULL || nb > 0) { ZVAL_STRINGL(v, b, nb, 1); } \
    else { ZVAL_EMPTY_STRING(v); }
#define zap_zval_long_p(v, n) \
    ZVAL_LONG(v, n)
#define zap_zval_array_p(v) \
    array_init(v)
#define zap_zval_zval_p(v, z, copy, dtor) \
    ZVAL_ZVAL(v, z, copy, dtor)
#define zap_zval_res_p(v, ptr, type) \
    ZEND_REGISTER_RESOURCE(v, ptr, type)

#define zap_zval_is_undef(v) \
    (v == NULL)
#define zap_zval_is_bool(v) \
    (Z_TYPE_P(v) == IS_BOOL)
#define zap_zval_is_array(v) \
    (Z_TYPE_P(v) == IS_ARRAY)

#define zap_zval_boolval(v)  \
    Z_BVAL_P(v)

// These all set a zapval to something
#define zapval_undef(v) \
    zap_zval_undef_p(v)
#define zapval_null(v) \
    zap_zval_null_p(v)
#define zapval_empty_string(v) \
    zap_zval_empty_string_p(v)
#define zapval_stringl(v, b, nb) \
    zap_zval_stringl_p(v, b, nb)
#define zapval_long(v, n) \
    zap_zval_long_p(v, n)
#define zapval_array(v) \
    zap_zval_array_p(v)
#define zapval_zval(v, z, copy, dtor) \
    zap_zval_zval_p(v, z, copy, dtor)
#define zapval_res(v, ptr, type) \
    zap_zval_res_p(v, ptr, type)

// These all allocate and set a zapval to something
#define zapval_alloc(v) \
    MAKE_STD_ZVAL(v)
#define zapval_addref(v) \
    Z_ADDREF_P(v)
#define zapval_destroy(v) \
    zval_ptr_dtor(&v)

#define zapval_is_undef(v) (v == NULL)
#define zapval_is_bool(v) (Z_TYPE_P(v) == IS_BOOL)
#define zapval_is_string(v) (Z_TYPE_P(v) == IS_STRING)
#define zapval_is_long(v) (Z_TYPE_P(v) == IS_LONG)
#define zapval_is_array(v) (Z_TYPE_P(v) == IS_ARRAY)

#define zapval_arrval(v) Z_ARRVAL_P(v)

#define zapval_strlen_p(v) Z_STRLEN_PP(v)
#define zapval_strval_p(v) Z_STRVAL_PP(v)
#define zapval_lval_p(v) Z_LVAL_PP(v)

#define zap_throw_exception_object(o) zend_throw_exception_object(o TSRMLS_CC)

static inline int _zap_get_parameters_array_ex(int param_count, zapval *args TSRMLS_DC)
{
    if (param_count <= 16) {
        int i;
        zval **_args[16];
        int retval = _zend_get_parameters_array_ex(param_count, _args TSRMLS_CC);
        for (i = 0; i < param_count; ++i) {
            args[i] = *_args[i];
        }
        return retval;
    } else {
        int i;
        zval ***_args = emalloc(param_count * sizeof(zval**));
        int retval = _zend_get_parameters_array_ex(param_count, _args TSRMLS_CC);
        for (i = 0; i < param_count; ++i) {
            args[i] = *_args[i];
        }
        efree(_args);
        return retval;
    }
}
#define zap_get_parameters_array_ex(param_count, args) \
    _zap_get_parameters_array_ex(param_count, args TSRMLS_CC)

#define zap_call_user_function(o, f, r, np, p) \
    call_user_function(CG(function_table), o, f, r, np, p TSRMLS_CC)

static inline zval * _zap_hash_str_add(HashTable *ht, char *key, size_t len, zval *pData) {
    if (zend_hash_add(ht, key, len + 1, (void*)&pData, sizeof(zval**), NULL) != SUCCESS) {
        return NULL;
    }
    return pData;
}
#define zap_hash_str_add(ht, k, nk, hv) \
    _zap_hash_str_add(ht, k, nk, hv)

static inline zval * _zap_hash_next_index_insert(HashTable *ht, zval *pData) {
    if (zend_hash_next_index_insert(
            ht, (void*)&pData, sizeof(zval**), NULL) != SUCCESS) {
        return NULL;
    }
    return pData;
}
#define zap_hash_next_index_insert(ht, hv) \
    _zap_hash_next_index_insert(ht, hv)

static inline zval * _zap_hash_str_find(HashTable *ht, char *key, size_t len) {
    zval **result = NULL;
    if (zend_hash_find(ht, key, len+1, (void**)&result) != SUCCESS) {
        return NULL;
    }
    return *result;
}
#define zap_hash_str_find(ht, k, nk) \
    _zap_hash_str_find(ht, k, nk)

static inline zapval * _zap_hash_index_find(HashTable *ht, ulong i) {
    zval **result;
    if (zend_hash_index_find(ht, i, (void**)&result) != SUCCESS) {
        return NULL;
    }
    return result;
}
#define zap_hash_index_find(ht, i) \
    _zap_hash_index_find(ht, i)

static inline zapval * _zap_hash_get_current_data_ex(HashTable *ht, HashPosition *pos) {
    zval **result;
    if (zend_hash_get_current_data_ex(ht, (void**)&result, pos) != SUCCESS) {
        return NULL;
    }
    return result;
}
#define zap_hash_get_current_data_ex(ht, pos) \
    _zap_hash_get_current_data_ex(ht, pos)

static inline int zap_hash_str_get_current_key_ex(HashTable *ht, char **str,
        uint *len, zend_ulong *num_index, HashPosition *pos) {
    uint len_out = 0;
    int key_type = zend_hash_get_current_key_ex(ht, str, &len_out, num_index, 0, pos);
    if (len != NULL) {
        *len = len_out - 1;
    }
    return key_type;
}

#endif

#define zapval_alloc_null(v) \
    zapval_alloc(v); \
    zapval_null(v)
#define zapval_alloc_empty_string(v) \
    zapval_alloc(v); \
    zapval_empty_string(v)
#define zapval_alloc_stringl(v, b, nb) \
    zapval_alloc(v); \
    zapval_stringl(v, b, nb)
#define zapval_alloc_long(v, n) \
    zapval_alloc(v); \
    zapval_long(v, n)
#define zapval_alloc_array(v) \
    zapval_alloc(v); \
    zapval_array(v)
#define zapval_alloc_zval(v, z, copy, dtor) \
    zapval_alloc(v); \
    zapval_zval(v, z, copy, dtor)
#define zapval_alloc_res(v, ptr, type) \
    zapval_alloc(v); \
    zapval_res(v, ptr, type)

#define zapval_is_undef_p(v) zapval_is_undef(*v)
#define zapval_is_bool_p(v) zapval_is_bool(*v)
#define zapval_is_string_p(v) zapval_is_string(*v)
#define zapval_is_long_p(v) zapval_is_long(*v)

#if PHP_VERSION_ID >= 50400
#define zap_object_properties_init object_properties_init
#else
static void zap_object_properties_init(zend_object *obj, zend_class_entry* type) {
    zval *tmp;
    ALLOC_HASHTABLE(obj->properties);
    zend_hash_init(obj->properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(obj->properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
}
#endif

#if ZEND_MODULE_API_NO >= 20151012
#define zap_zend_register_internal_class_ex(ce, parent_ce) zend_register_internal_class_ex(ce, parent_ce TSRMLS_CC)
#else
#define zap_zend_register_internal_class_ex(ce, parent_ce) zend_register_internal_class_ex(ce, parent_ce, NULL TSRMLS_CC)
#endif

#if ZEND_MODULE_API_NO >= 20060613
#define zap_zend_exception_get_default() zend_exception_get_default(TSRMLS_C)
#else
#define zap_zend_exception_get_default() zend_exception_get_default()
#endif

#endif // ZAP_H_
