#ifndef PARAMPARSER_H_
#define PARAMPARSER_H_

#define PCBC_PP_MAX_ARGS 10

typedef struct {
    char *str;
    uint len;
} pcbc_pp_id;

typedef struct {
    char name[16];
    zval **ptr;
    zapval val;
} pcbc_pp_state_arg;

typedef struct {
    pcbc_pp_state_arg args[PCBC_PP_MAX_ARGS];
    int arg_req;
    int arg_opt;
    int arg_named;
    int cur_idx;
    zapval zids;
    HashPosition hash_pos;
} pcbc_pp_state;

// assumes first parameter in the spec is the ids (`id|`).
int pcbc_pp_begin(int param_count TSRMLS_DC, pcbc_pp_state *state, const char *spec, ...) {
    zapval args[PCBC_PP_MAX_ARGS];
    char arg_name[16];
    const char *spec_iter = spec;
    char *arg_iter = arg_name;
    int arg_type = 0;
    int arg_num = 0;
    zapval *znamed;
    int arg_unnamed;
    int ii;
    va_list vl;
    va_start(vl,spec);

    if (zap_get_parameters_array_ex(param_count, args) != SUCCESS) {
        return FAILURE;
    }

    state->zids = args[0];
    state->cur_idx = 0;
    state->arg_req = 0;
    state->arg_opt = 0;
    state->arg_named = 0;

    do {
        if (*spec_iter == 0 || *spec_iter == ',' || *spec_iter == '|') {
            if (arg_iter != arg_name) {
                pcbc_pp_state_arg *arg = &state->args[arg_num];
                *arg_iter = 0;

                // First arguement (id) is a special case...
                if (arg_num == 0) {
                    // First arguement (id) is a special case...
                    if (strcmp(arg_name, "id") != 0) {
                        php_printf("First argument must be ID.\n");
                        return FAILURE;
                    }
                }

                memcpy(arg->name, arg_name, 16);

                arg->ptr = va_arg(vl, zval**);

                if (arg_num > 0 && arg_num < param_count && arg_type < 2) {
                    arg->val = args[arg_num];
                } else {
                    zapval_undef(arg->val);
                }

                if (arg_type == 0) {
                    state->arg_req++;
                } else if (arg_type == 1) {
                    state->arg_opt++;
                } else if (arg_type == 2) {
                    state->arg_named++;
                }

                arg_num++;
            }

            if (*spec_iter == '|') {
                if (arg_type < 2) {
                    arg_type++;
                }
            }
            if (*spec_iter == 0) {
                break;
            }
            arg_iter = arg_name;
        } else {
            *arg_iter++ = *spec_iter;
        }

        spec_iter++;
    } while(1);

    if (param_count < state->arg_req) {
        // TODO: This should not printf...
        php_printf("Less than required number of args.\n");
        return FAILURE;
    }

    arg_unnamed = state->arg_req + state->arg_opt;

    if (param_count > arg_unnamed) {
        znamed = &args[arg_unnamed];

        // Ensure that it is an options array!
        if (!zapval_is_array(*znamed)) {
            php_printf("Options argument must be an associative array.\n");
            return FAILURE;
        }
    } else {
        znamed = NULL;
    }

    for (ii = 0; ii < state->arg_named; ++ii) {
        int aii = arg_unnamed + ii;
        pcbc_pp_state_arg *arg = &state->args[aii];

        if (znamed) {
            HashTable *htoptions = zapval_arrval(*znamed);
            zval *zvalue = zap_hash_str_find(
                    htoptions, arg->name, strlen(arg->name));

            if (zvalue) {
                arg->val = zapval_from_zvalptr(zvalue);
            } else {
                zapval_undef(arg->val);
            }
        } else {
            zapval_undef(arg->val);
        }
    }

    if (zapval_is_array(state->zids)) {
        // If this is an array, make sure its internal pointer is the start.
        HashTable *hash = zapval_arrval(state->zids);
        zend_hash_internal_pointer_reset_ex(hash, &state->hash_pos);
    } else if (zapval_is_string(state->zids)) {
        // Nothing to configure for basic string
    } else {
        // Definitely an error
        return FAILURE;
    }

    return SUCCESS;
}

int pcbc_pp_ismapped(pcbc_pp_state *state) {
    return !zapval_is_string(state->zids);
}

int pcbc_pp_keycount(pcbc_pp_state *state) {
    if (zapval_is_string(state->zids)) {
        return 1;
    } else if (zapval_is_array(state->zids)) {
        return zend_hash_num_elements(zapval_arrval(state->zids));
    } else {
        return 0;
    }
}

int pcbc_pp_next(pcbc_pp_state *state) {
    int ii;
    int arg_total = state->arg_req + state->arg_opt + state->arg_named;
    pcbc_pp_id *id_ptr = (pcbc_pp_id*)state->args[0].ptr;

    // Set everything to 'base' values
    for (ii = 1; ii < arg_total; ++ii) {
        if (zapval_is_undef(state->args[ii].val)) {
            *(state->args[ii].ptr) = NULL;
        } else {
            *(state->args[ii].ptr) = zapval_zvalptr(state->args[ii].val);
        }
    }

    if (zapval_is_array(state->zids)) {
        HashTable *hash = zapval_arrval(state->zids);
        zapval *data;
        zend_ulong keyidx, key_type;
        char *keystr;
        uint keystr_len;

        data = zap_hash_get_current_data_ex(hash, &state->hash_pos);
        if (data == 0) {
            return 0;
        }

        key_type = zap_hash_str_get_current_key_ex(
                hash, &keystr, &keystr_len, &keyidx, &state->hash_pos);

        if (key_type == HASH_KEY_IS_STRING) {
            id_ptr->str = keystr;
            id_ptr->len = keystr_len;

            if (zapval_is_array(*data)) {
                HashTable *htdata = zapval_arrval(*data);
                zval *zvalue;

                for (ii = 1; ii < arg_total; ++ii) {
                    pcbc_pp_state_arg * arg = &state->args[ii];

                    zvalue = zap_hash_str_find(
                            htdata, arg->name, strlen(arg->name));
                    if (zvalue != NULL) {
                        *(arg->ptr) = zvalue;
                    }
                }
            }
        } else if (key_type == HASH_KEY_IS_LONG) {
            id_ptr->str = zapval_strval_p(data);
            id_ptr->len = zapval_strlen_p(data);
        }

        zend_hash_move_forward_ex(hash, &state->hash_pos);
        return 1;
    } else if (zapval_is_string(state->zids)) {
        if (state->cur_idx > 0) {
            return 0;
        }
        id_ptr->str = zapval_strval_p(&state->zids);
        id_ptr->len = zapval_strlen_p(&state->zids);
        state->cur_idx++;
        return 1;
    } else {
        // Invalid type for state->zids
        return 0;
    }
}

#endif
