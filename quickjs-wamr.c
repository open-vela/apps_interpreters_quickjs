#include "quickjs.h"
#include "quickjs-wamr.h"

JSValue JS_GetGlobalVar_wrapper(JSContext *ctx, JSAtom prop,
                               JS_BOOL throw_ref_error)
{
    return JS_GetGlobalVar(ctx, prop, throw_ref_error);
}

JSValue JS_CallConstructorInternal_wrapper(JSContext *ctx,
                                   JSValueConst func_obj,
                                   JSValueConst new_target,
                                   int argc, JSValue *argv, int flags)
{
    return JS_CallConstructorInternal(ctx, func_obj, new_target, argc, argv, flags);
}

JSAtom find_atom_wrapper(JSContext *ctx, const char *name)
{
    return find_atom(ctx, name);
}

int set_array_length_wrapper(JSContext *ctx, JSObject *p, JSValue val,
                      int flags)
{
    return set_array_length(ctx, p, val, flags);
}

int JS_DefinePropertyDesc_wrapper(JSContext *ctx, JSValueConst obj, JSAtom prop,
                           JSValueConst desc, int flags) {
    return JS_DefinePropertyDesc(ctx, obj, prop, desc, flags);
}

int js_operator_typeof_wrapper(JSContext *ctx, JSValueConst op1) {
    return js_operator_typeof(ctx, op1);
}

void JS_Dump_wrapper(JSRuntime *rt, JSValue *p) {
    if (JS_IsObject(*p)) {
        JSObject *js_obj = JS_VALUE_GET_OBJ(*p);
        JS_DumpObject(rt, js_obj);
    } else {
        JS_DumpValueShort(rt, *p);
    }
}

static int JS_DumpValueShortWithBuffer(JSRuntime *rt, JSValueConst val, char *buffer, uint32_t len)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(val);
    const char *str;

    char *cur = buffer;
    uint32_t remain = len;
    int res = -1;

    switch(tag) {
    case JS_TAG_INT:
        res = snprintf(cur, remain, "%d", JS_VALUE_GET_INT(val));
        if (res >= remain) {
            return -1;
        }
        break;
    case JS_TAG_BOOL:
        if (JS_VALUE_GET_BOOL(val))
            str = "true";
        else
            str = "false";
        goto print_str;
    case JS_TAG_NULL:
        str = "null";
        goto print_str;
    case JS_TAG_EXCEPTION:
        str = "exception";
        goto print_str;
    case JS_TAG_UNINITIALIZED:
        str = "uninitialized";
        goto print_str;
    case JS_TAG_UNDEFINED:
        str = "undefined";
    print_str:
        res = snprintf(cur, remain, "%s", str);
        if (res >= remain) {
            return -1;
        }
        break;
    case JS_TAG_FLOAT64:
        res = snprintf(cur, remain, "%.14g", JS_VALUE_GET_FLOAT64(val));
        if (res >= remain) {
            return -1;
        }
        break;
#ifdef CONFIG_BIGNUM
    case JS_TAG_BIG_INT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            char *str;
            str = bf_ftoa(NULL, &p->num, 10, 0,
                          BF_RNDZ | BF_FTOA_FORMAT_FRAC);
            printf("%sn", str);
            bf_realloc(&rt->bf_ctx, str, 0);
        }
        break;
    case JS_TAG_BIG_FLOAT:
        {
            JSBigFloat *p = JS_VALUE_GET_PTR(val);
            char *str;
            str = bf_ftoa(NULL, &p->num, 16, BF_PREC_INF,
                          BF_RNDZ | BF_FTOA_FORMAT_FREE | BF_FTOA_ADD_PREFIX);
            printf("%sl", str);
            bf_free(&rt->bf_ctx, str);
        }
        break;
    case JS_TAG_BIG_DECIMAL:
        {
            JSBigDecimal *p = JS_VALUE_GET_PTR(val);
            char *str;
            str = bfdec_ftoa(NULL, &p->num, BF_PREC_INF,
                             BF_RNDZ | BF_FTOA_FORMAT_FREE);
            printf("%sm", str);
            bf_free(&rt->bf_ctx, str);
        }
        break;
#endif
    case JS_TAG_STRING:
        {
            JSString *p;
            p = JS_VALUE_GET_STRING(val);
            // JS_DumpString(rt, p);

            int i, c, sep;

            if (p == NULL) {
                res = snprintf(cur, remain, "<null>");
                if (res >= remain) {
                    return -1;
                }
                cur = cur + strlen(buffer);
                remain = len - strlen(buffer);
                break;;
            }
            res = snprintf(cur, remain, "%d", p->header.ref_count);
            if (res >= remain) {
                return -1;
            }
            cur = buffer + strlen(buffer);
            remain = len - strlen(buffer);

            sep = (p->header.ref_count == 1) ? '\"' : '\'';

            res = snprintf(cur, remain, (p->header.ref_count == 1) ? "\"" : "\'");
            if (res >= remain) {
                return -1;
            }
            cur = buffer + strlen(buffer);
            remain = len - strlen(buffer);

            for(i = 0; i < p->len; i++) {
                if (p->is_wide_char)
                    c = p->u.str16[i];
                else
                    c = p->u.str8[i];
                if (c == sep || c == '\\') {
                    res = snprintf(cur, remain, "\\%c", c);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else if (c >= ' ' && c <= 126) {
                    res = snprintf(cur, remain, "%c", c);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else if (c == '\n') {
                    res = snprintf(cur, remain, "\\n");
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else {
                    res = snprintf(cur, remain, "\\u%04x", c);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                }
            }
            res = snprintf(cur, remain, (p->header.ref_count == 1) ? "\"" : "\'");
            if (res >= remain) {
                return -1;
            }
            cur = buffer + strlen(buffer);
            remain = len - strlen(buffer);
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(val);
            char buf[ATOM_GET_STR_BUF_SIZE];
            res = snprintf(cur, remain, "[bytecode %s]", JS_AtomGetStrRT(rt, buf, sizeof(buf), b->func_name));
            if (res >= remain) {
                return -1;
            }
        }
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            JSAtom atom = rt->class_array[p->class_id].class_name;
            char atom_buf[ATOM_GET_STR_BUF_SIZE];
            res = snprintf(cur, remain, "[%s %p]",
                     JS_AtomGetStrRT(rt, atom_buf, sizeof(atom_buf), atom), (void *)p);
            if (res >= remain) {
                return -1;
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            char atom_buf[ATOM_GET_STR_BUF_SIZE];
            res = snprintf(cur, remain, "Symbol(%s)",
                     JS_AtomGetStrRT(rt, atom_buf, sizeof(atom_buf), js_get_atom_index(rt, p)));
            if (res >= remain) {
                return -1;
            }
        }
        break;
    case JS_TAG_MODULE:
        res = snprintf(cur, remain, "[module]");
        if (res >= remain) {
            return -1;
        }
        break;
    case JS_TAG_EXT_OBJ:
        res = snprintf(cur, remain, "EXT_OBJ: %p", JS_VALUE_GET_PTR(val));
        if (res >= remain) {
            return -1;
        }
        break;
    case JS_TAG_EXT_FUNC:
        res = snprintf(cur, remain, "EXT_FUNC: %p", JS_VALUE_GET_PTR(val));
        if (res >= remain) {
            return -1;
        }
        break;
    case JS_TAG_EXT_INFC:
        res = snprintf(cur, remain, "EXT_INFC: %p", JS_VALUE_GET_PTR(val));
        if (res >= remain) {
            return -1;
        }
        break;
    default:
        res = snprintf(cur, remain, "[unknown tag %d]", tag);
        if (res >= remain) {
            return -1;
        }
        break;
    }
    return strlen(buffer) + 1;
}

static int JS_DumpObjectWithBuffer(JSRuntime *rt, JSObject *p, char *buffer, uint32_t len)
{
    uint32_t i;
    char atom_buf[ATOM_GET_STR_BUF_SIZE];
    JSShape *sh;
    JSShapeProperty *prs;
    JSProperty *pr;
    BOOL is_first = TRUE;

    /* XXX: should encode atoms with special characters */
    sh = p->shape; /* the shape can be NULL while freeing an object */

    char *cur = buffer;
    uint32_t remain = len;
    int res = -1;

    res = snprintf(cur, remain, "%10s ",
             JS_AtomGetStrRT(rt, atom_buf, sizeof(atom_buf), rt->class_array[p->class_id].class_name));
    if (res >= remain) {
        return -1;
    }
    cur = buffer + strlen(buffer);
    remain = len - strlen(buffer);

    if (p->is_exotic && p->fast_array) {
        res = snprintf(cur, remain, "[ ");
        if (res >= remain) {
            return -1;
        }
        cur = buffer + strlen(buffer);
        remain = len - strlen(buffer);
        for(i = 0; i < p->u.array.count; i++) {
            if (i != 0) {
                res = snprintf(cur, remain, ", ");
                if (res >= remain) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
            }
            switch (p->class_id) {
            case JS_CLASS_ARRAY:
            case JS_CLASS_ARGUMENTS:
                if (JS_DumpValueShortWithBuffer(rt, p->u.array.u.values[i], cur, remain) == -1) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
                break;
            case JS_CLASS_UINT8C_ARRAY:
            case JS_CLASS_INT8_ARRAY:
            case JS_CLASS_UINT8_ARRAY:
            case JS_CLASS_INT16_ARRAY:
            case JS_CLASS_UINT16_ARRAY:
            case JS_CLASS_INT32_ARRAY:
            case JS_CLASS_UINT32_ARRAY:
#ifdef CONFIG_BIGNUM
            case JS_CLASS_BIG_INT64_ARRAY:
            case JS_CLASS_BIG_UINT64_ARRAY:
#endif
            case JS_CLASS_FLOAT32_ARRAY:
            case JS_CLASS_FLOAT64_ARRAY:
                {
                    int size = 1 << typed_array_size_log2(p->class_id);
                    const uint8_t *b = p->u.array.u.uint8_ptr + i * size;
                    while (size-- > 0) {
                        res = snprintf(cur, remain, "%02X", *b++);
                        if (res >= remain) {
                            return -1;
                        }
                        cur = cur + strlen(buffer);
                        remain = len - strlen(buffer);
                    }
                }
                break;
            }
        }
        res = snprintf(cur, remain, " ] ");
        if (res >= remain) {
            return -1;
        }
        cur = cur + strlen(buffer);
        remain = len - strlen(buffer);
    }

    if (sh) {
        res = snprintf(cur, remain, "{ ");
        if (res >= remain) {
            return -1;
        }
        cur = buffer + strlen(buffer);
        remain = len - strlen(buffer);
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->atom != JS_ATOM_NULL) {
                pr = &p->prop[i];
                if (!is_first) {
                    res = snprintf(cur, remain, ", ");
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                }

                res = snprintf(cur, remain, "%s: ",
                               JS_AtomGetStrRT(rt, atom_buf, sizeof(atom_buf), prs->atom));
                if (res >= remain) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    res = snprintf(cur, remain, "[getset %p %p]", (void *)pr->u.getset.getter,
                                   (void *)pr->u.getset.setter);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    res = snprintf(cur, remain, "[varref %p]", (void *)pr->u.var_ref);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                    res = snprintf(cur, remain, "[autoinit %p %d %p]", (void *)js_autoinit_get_realm(pr),
                                   js_autoinit_get_id(pr), (void *)pr->u.init.opaque);
                    if (res >= remain) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                } else {
                    if (JS_DumpValueShortWithBuffer(rt, pr->u.value, cur, remain) == -1) {
                        return -1;
                    }
                    cur = buffer + strlen(buffer);
                    remain = len - strlen(buffer);
                }
                is_first = FALSE;
            }
        }
        res = snprintf(cur, remain, " }");
        if (res >= remain) {
            return -1;
        }
        cur = buffer + strlen(buffer);
        remain = len - strlen(buffer);
    }

    if (js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        JSVarRef **var_refs;
        if (b->closure_var_count) {
            var_refs = p->u.func.var_refs;
            res = snprintf(cur, remain, " Closure:");
            if (res >= remain) {
                return -1;
            }
            cur = buffer + strlen(buffer);
            remain = len - strlen(buffer);
            for(i = 0; i < b->closure_var_count; i++) {
                res = snprintf(cur, remain, " ");
                if (res >= remain) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
                if (JS_DumpValueShortWithBuffer(rt, var_refs[i]->value, cur, remain) == -1) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
            }
            if (p->u.func.home_object) {
                res = snprintf(cur, remain, " HomeObject: ");
                if (res >= remain) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
                if (JS_DumpValueShortWithBuffer(rt, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object),
                    cur, remain) == -1) {
                    return -1;
                }
                cur = buffer + strlen(buffer);
                remain = len - strlen(buffer);
            }
        }
    }
    res = snprintf(cur, remain, "\n");
    if (res >= remain) {
        return -1;
    }
    return strlen(buffer) + 1;
}

int JS_DumpWithBuffer(JSRuntime *rt, JSValue *p, void *buffer, uint32_t len) {
    char *buf = (char *)buffer;
    if (JS_IsObject(*p)) {
        JSObject *js_obj = JS_VALUE_GET_OBJ(*p);
        return JS_DumpObjectWithBuffer(rt, js_obj, buf, len);
    }
    return JS_DumpValueShortWithBuffer(rt, *p, buf, len);
}

int JS_OrdinaryIsInstanceOf_wrapper(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj) {
    return JS_OrdinaryIsInstanceOf(ctx, val, obj);
}

uint32_t getClassIdFromObject(JSObject *obj) {
    return obj->class_id;
}

JSClassCall* getCallByClassId(JSRuntime *rt, uint32_t classId) {
    return rt->class_array[classId].call;
}