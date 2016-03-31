#ifndef OPCOOKIE_H_
#define OPCOOKIE_H_

#include <php.h>
#include "couchbase.h"
#include "zap.h"

typedef struct {
    void *next;
    lcb_error_t err;
} opcookie_res;

typedef struct {
    opcookie_res *res_head;
    opcookie_res *res_tail;
    lcb_error_t first_error;
} opcookie;

opcookie * opcookie_init();
void opcookie_destroy(opcookie *cookie);
void opcookie_push(opcookie *cookie, opcookie_res *res);
lcb_error_t opcookie_get_first_error(opcookie *cookie);
opcookie_res * opcookie_next_res(opcookie *cookie, opcookie_res *cur);

#define FOREACH_OPCOOKIE_RES(Type, Res, cookie) \
    Res = NULL; \
    while ((Res = (Type*)opcookie_next_res(cookie, (opcookie_res*)Res)) != NULL)

#endif /* OPCOOKIE_H_ */
