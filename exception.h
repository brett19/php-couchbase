#ifndef EXCEPTION_H_
#define EXCEPTION_H_

zval * create_exception(zend_class_entry *exception_ce, const char *message, long code TSRMLS_DC);
zval * create_lcb_exception(long code TSRMLS_DC);

extern zend_class_entry *default_exception_ce;
extern zend_class_entry *cb_exception_ce;

#endif
