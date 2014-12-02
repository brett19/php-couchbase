#ifndef EXCEPTION_H_
#define EXCEPTION_H_

zval * create_exception(zend_class_entry *exception_ce, const char *message, long code TSRMLS_DC);
zval * create_pcbc_exception(const char *message, long code TSRMLS_DC);
zval * create_lcb_exception(long code TSRMLS_DC);

#define throw_pcbc_exception(message, code) \
	zend_throw_exception_object(create_pcbc_exception(message, code TSRMLS_CC) TSRMLS_CC);
#define throw_lcb_exception(code) \
	zend_throw_exception_object(create_lcb_exception(code TSRMLS_CC) TSRMLS_CC);

extern zend_class_entry *default_exception_ce;
extern zend_class_entry *cb_exception_ce;

#endif
