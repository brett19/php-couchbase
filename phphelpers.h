#ifndef PHPHELPERS_H_
#define PHPHELPERS_H_

#include <php.h>

#if PHP_VERSION_ID >= 50400
#define phlp_object_properties_init object_properties_init
#else
static void phlp_object_properties_init(zend_object *obj, zend_class_entry* type) {
	zval *tmp;
	zend_hash_copy(obj->properties, &type->default_properties,
		(copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));
}
#endif

#endif // PHPHELPERS_H_
