#include "opcookie.h"
#include "exception.h"
#include "metadoc.h"
#include "zap.h"

opcookie * opcookie_init()
{
    return ecalloc(1, sizeof(opcookie));
}

void opcookie_destroy(opcookie *cookie) {
    opcookie_res *iter = cookie->res_head;
    while (iter != NULL) {
        opcookie_res *cur = iter;
        iter = cur->next;
        efree(cur);
    }
    efree(cookie);
}

lcb_error_t opcookie_get_first_error(opcookie *cookie)
{
    return cookie->first_error;
}

void opcookie_push(opcookie *cookie, opcookie_res *res)
{
    if (cookie->res_head == NULL) {
        cookie->res_head = res;
        cookie->res_tail = res;
    } else {
        cookie->res_tail->next = res;
        cookie->res_tail = res;
    }
    res->next = NULL;

    if (res->err != LCB_SUCCESS && cookie->first_error == LCB_SUCCESS) {
        cookie->first_error = res->err;
    }
}

opcookie_res * opcookie_next_res(opcookie *cookie, opcookie_res *cur)
{
    if (cur == NULL) {
        return cookie->res_head;
    } else {
        return cur->next;
    }
}
