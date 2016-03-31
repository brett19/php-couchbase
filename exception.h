#ifndef EXCEPTION_H_
#define EXCEPTION_H_

#include "zap.h"

void make_exception(zapval *ex, zend_class_entry *exception_ce, const char *message, long code TSRMLS_DC);
void make_pcbc_exception(zapval *ex, const char *message, long code TSRMLS_DC);
void make_lcb_exception(zapval *ex, long code, const char *msg TSRMLS_DC);

#define throw_pcbc_exception(message, code) { \
    zapval zerror; \
    make_pcbc_exception(&zerror, message, code TSRMLS_CC); \
    zap_throw_exception_object(zerror); }
#define throw_lcb_exception(code) { \
    zapval zerror; \
    make_lcb_exception(&zerror, code, NULL TSRMLS_CC); \
    zap_throw_exception_object(zerror); }

extern zend_class_entry *default_exception_ce;
extern zend_class_entry *cb_exception_ce;

#endif
