/*
 * Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "quickjs.h"

#ifdef __BYTECODE_OPTIMIZATION__
void load_atom_array(JSRuntime* rt, const uint8_t* buffer)
{
    if (buffer == NULL) {
        return;
    }
    int32_t pos = 0;
    rt->atom_hash_size = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);
    rt->atom_size = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);
    rt->atom_count = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);
    rt->atom_count_resize = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);
    rt->atom_free_index = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);

    rt->atom_hash = js_malloc_rt(rt, sizeof(uint32_t) * rt->atom_hash_size);
    if (!rt->atom_hash) {
        return;
    }
    memcpy(rt->atom_hash, buffer + pos, sizeof(uint32_t) * rt->atom_hash_size);
    pos += sizeof(uint32_t) * rt->atom_hash_size;

    rt->atom_array = js_malloc_rt(rt, sizeof(uint32_t) * rt->atom_size);
    if (!rt->atom_array) {
        return;
    }
    memcpy(rt->atom_array, buffer + pos, sizeof(JSAtomStruct*) * rt->atom_size);
    pos += sizeof(JSAtomStruct*) * rt->atom_size;

    int32_t jsstring_all_size = 0;
    jsstring_all_size = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);

    uint8_t* jsstring_all_buffer = js_malloc_rt(rt, jsstring_all_size);
    if (!jsstring_all_buffer) {
        return;
    }
    memcpy(jsstring_all_buffer, buffer + pos, jsstring_all_size);

    int32_t len = sizeof(int32_t);
    int32_t str_pos = 0;
    for (int i = 0; i < rt->atom_size; i++) {
        if (!atom_is_free(rt->atom_array[i])) {
            rt->atom_array[i] = (JSAtomStruct*)(jsstring_all_buffer + str_pos + len);
            str_pos += *(int32_t*)(jsstring_all_buffer + str_pos);
            _g_const_atom_count = i;
#ifdef DUMP_LEAKS
            list_add_tail(&rt->atom_array[i]->link, &rt->string_list);
#endif
        }
    }
    _g_const_atom_count++;
    rt->atom_free_index = _g_const_atom_count;
}

static int JS_InitAtoms_Ex(JSRuntime* rt, const uint8_t* rt_str_info)
{
    load_atom_array(rt, rt_str_info);
    return 0;
}

void JS_FreeRuntime_Ex(JSRuntime* rt)
{
#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG
    js_debugger_free(rt, &rt->debugger_info);
#endif

    struct list_head *el, *el1;
    int i;

    JS_FreeValueRT(rt, rt->current_exception);
#ifdef CONFIG_MEMORY_LEAK_TRACK
    if (rt->newObjVector) {
        vector_free(rt->newObjVector);
        rt->newObjVector = NULL;
    }
#endif
    list_for_each_safe(el, el1, &rt->job_list)
    {
        JSJobEntry* e = list_entry(el, JSJobEntry, link);
        for (i = 0; i < e->argc; i++)
            JS_FreeValueRT(rt, e->argv[i]);
        js_free_rt(rt, e);
    }
    init_list_head(&rt->job_list);

    JS_RunGC(rt);

    /* leaking objects */
    {
        BOOL header_done;
        JSGCObjectHeader* p;
        int count;

        /* remove the internal refcounts to display only the object
           referenced externally */
        list_for_each(el, &rt->gc_obj_list)
        {
            p = list_entry(el, JSGCObjectHeader, link);
            p->mark = 0;
        }
        gc_decref(rt);

        header_done = FALSE;
        list_for_each(el, &rt->gc_obj_list)
        {
            p = list_entry(el, JSGCObjectHeader, link);
            if (p->ref_count != 0) {
                if (!header_done) {
                    printf("Object leaks:\n");
                    JS_DumpObjectHeader(rt);
                    header_done = TRUE;
                }
                JS_DumpGCObject(rt, p);
            }
        }

        count = 0;
        list_for_each(el, &rt->gc_obj_list)
        {
            p = list_entry(el, JSGCObjectHeader, link);
            if (p->ref_count == 0) {
                count++;
            }
        }
        if (count != 0)
            printf("Secondary object leaks: %d\n", count);
    }

    /* free the classes */
    for (i = 0; i < rt->class_count; i++) {
        JSClass* cl = &rt->class_array[i];
        if (cl->class_id != 0) {
            JS_FreeAtomRT(rt, cl->class_name);
        }
    }
    js_free_rt(rt, rt->class_array);

#ifdef CONFIG_BIGNUM
    bf_context_end(&rt->bf_ctx);
#endif

#ifdef DUMP_LEAKS
    /* only the atoms defined in JS_InitAtoms() should be left */
    {
        BOOL header_done = FALSE;

        for (i = 0; i < rt->atom_size; i++) {
            JSAtomStruct* p = rt->atom_array[i];
            if (!atom_is_free(p) /* && p->str*/) {
                if (i >= _g_const_atom_count || p->header.ref_count != 1) {
                    if (!header_done) {
                        header_done = TRUE;
                        if (rt->rt_info) {
                            printf("%s:1: atom leakage:", rt->rt_info);
                        } else {
                            printf("Atom leaks:\n"
                                   "    %6s %6s %s\n",
                                "ID", "REFCNT", "NAME");
                        }
                    }
                    if (rt->rt_info) {
                        printf(" ");
                    } else {
                        printf("    %6u %6u ", i, p->header.ref_count);
                    }
                    switch (p->atom_type) {
                    case JS_ATOM_TYPE_STRING:
                        JS_DumpString(rt, p);
                        break;
                    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
                        printf("Symbol.for(");
                        JS_DumpString(rt, p);
                        printf(")");
                        break;
                    case JS_ATOM_TYPE_SYMBOL:
                        if (p->hash == JS_ATOM_HASH_SYMBOL) {
                            printf("Symbol(");
                            JS_DumpString(rt, p);
                            printf(")");
                        } else {
                            printf("Private(");
                            JS_DumpString(rt, p);
                            printf(")");
                        }
                        break;
                    }
                    if (rt->rt_info) {
                        printf(":%u", p->header.ref_count);
                    } else {
                        printf("\n");
                    }
                }
            }
        }
        if (rt->rt_info && header_done)
            printf("\n");
    }
#endif

    /* free the atoms */
#ifdef DUMP_LEAKS
    for (i = 0; i < _g_const_atom_count; i++) {
        JSAtomStruct* p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            list_del(&p->link);
        }
    }
#endif
    for (i = _g_const_atom_count; i < rt->atom_size; i++) {
        JSAtomStruct* p = rt->atom_array[i];
        if (!atom_is_free(p)) {
#ifdef DUMP_LEAKS
            list_del(&p->link);
#endif
            js_free_rt(rt, p);
        }
    }

    // 释放字节码优化块
    js_free_rt(rt, ((uint8_t*)rt->atom_array[0] - sizeof(int32_t)));
    js_free_rt(rt, rt->atom_array);
    js_free_rt(rt, rt->atom_hash);
    js_free_rt(rt, rt->shape_hash);
#ifdef DUMP_LEAKS
    if (!list_empty(&rt->string_list)) {
        if (rt->rt_info) {
            printf("%s:1: string leakage:", rt->rt_info);
        } else {
            printf("String leaks:\n"
                   "    %6s %s\n",
                "REFCNT", "VALUE");
        }
        list_for_each_safe(el, el1, &rt->string_list)
        {
            JSString* str = list_entry(el, JSString, link);
            if (rt->rt_info) {
                printf(" ");
            } else {
                printf("    %6u ", str->header.ref_count);
            }
            JS_DumpString(rt, str);
            if (rt->rt_info) {
                printf(":%u", str->header.ref_count);
            } else {
                printf("\n");
            }
            list_del(&str->link);
            js_free_rt(rt, str);
        }
        if (rt->rt_info)
            printf("\n");
    }
    {
        JSMallocState* s = &rt->malloc_state;
        if (s->malloc_count > 1) {
            if (rt->rt_info)
                printf("%s:1: ", rt->rt_info);
            printf("Memory leak: %" PRIu64 " bytes lost in %" PRIu64 " block%s\n",
                (uint64_t)(s->malloc_size - sizeof(JSRuntime)),
                (uint64_t)(s->malloc_count - 1), &"s"[s->malloc_count == 2]);
        }
    }
#endif

    {
        JSMallocState ms = rt->malloc_state;
        rt->mf.js_free(&ms, rt);
    }
}

JSRuntime* JS_NewRuntime_Ex2(const JSMallocFunctions* mf, void* opaque, const uint8_t* rt_str_info)
{
    JSRuntime* rt;
    JSMallocState ms;

    memset(&ms, 0, sizeof(ms));
    ms.opaque = opaque;
    ms.malloc_limit = -1;

    rt = mf->js_malloc(&ms, sizeof(JSRuntime));
    if (!rt)
        return NULL;
    memset(rt, 0, sizeof(*rt));
    rt->mf = *mf;
    if (!rt->mf.js_malloc_usable_size) {
        /* use dummy function if none provided */
        rt->mf.js_malloc_usable_size = js_malloc_usable_size_unknown;
    }
    rt->malloc_state = ms;
    rt->malloc_gc_threshold = 256 * 1024;

#ifdef CONFIG_BIGNUM
    bf_context_init(&rt->bf_ctx, js_bf_realloc, rt);
    set_dummy_numeric_ops(&rt->bigint_ops);
    set_dummy_numeric_ops(&rt->bigfloat_ops);
    set_dummy_numeric_ops(&rt->bigdecimal_ops);
#endif

    init_list_head(&rt->context_list);
    init_list_head(&rt->gc_obj_list);
    init_list_head(&rt->gc_zero_ref_count_list);
    rt->gc_phase = JS_GC_PHASE_NONE;

#ifdef DUMP_LEAKS
    init_list_head(&rt->string_list);
#endif
    init_list_head(&rt->job_list);

    if (JS_InitAtoms_Ex(rt, rt_str_info))
        goto fail;

    /* create the object, array and function classes */
    if (init_class_range(rt, js_std_class_def, JS_CLASS_OBJECT,
            countof(js_std_class_def))
        < 0)
        goto fail;
    rt->class_array[JS_CLASS_ARGUMENTS].exotic = &js_arguments_exotic_methods;
    rt->class_array[JS_CLASS_STRING].exotic = &js_string_exotic_methods;
    rt->class_array[JS_CLASS_MODULE_NS].exotic = &js_module_ns_exotic_methods;

    rt->class_array[JS_CLASS_C_FUNCTION].call = js_call_c_function;
    rt->class_array[JS_CLASS_C_FUNCTION_DATA].call = js_c_function_data_call;
    rt->class_array[JS_CLASS_BOUND_FUNCTION].call = js_call_bound_function;
    rt->class_array[JS_CLASS_GENERATOR_FUNCTION].call = js_generator_function_call;
#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG
    rt->dump_memory_info.is_started_memory_tracking = 0;
    rt->dump_memory_info.is_memory_tracking_on_timer_started = 0;
#endif
    if (init_shape_hash(rt))
        goto fail;

    rt->stack_size = JS_DEFAULT_STACK_SIZE;
    JS_UpdateStackTop(rt);

    rt->current_exception = JS_NULL;

    return rt;
fail:
    JS_FreeRuntime_Ex(rt);
    return NULL;
}

JSRuntime* JS_NewRuntime_Ex(const uint8_t* rt_str_info)
{
    return JS_NewRuntime_Ex2(&def_malloc_funcs, NULL, rt_str_info);
}

static int bc_get_atom_ex(BCReaderState* s, JSAtom* patom)
{
    uint32_t v;
    if (bc_get_leb128(s, &v))
        return -1;
    if (v & 1) {
        *patom = __JS_AtomFromUInt32(v >> 1);
        return 0;
    } else {
        *patom = v >> 1;
        return 0;
    }
}

static int JS_ReadFunctionBytecode_Ex(BCReaderState* s, JSFunctionBytecode* b,
    int byte_code_offset, uint32_t bc_len)
{
    uint8_t* bc_buf;
    int pos, len, op;
    uint32_t idx;

    if (s->is_rom_data) {
        /* directly use the input buffer */
        if (unlikely(s->buf_end - s->ptr < bc_len))
            return bc_read_error_end(s);
        bc_buf = (uint8_t*)s->ptr;
        s->ptr += bc_len;
    } else {
        bc_buf = (void*)((uint8_t*)b + byte_code_offset);
        if (bc_get_buf(s, bc_buf, bc_len))
            return -1;
    }
    b->byte_code_buf = bc_buf;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        len = short_opcode_info(op).size;
        switch (short_opcode_info(op).fmt) {
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
        case OP_FMT_atom_u16:
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            idx = get_u32(bc_buf + pos + 1);
            if (s->is_rom_data) {
                /* just increment the reference count of the atom */
                JS_DupAtom(s->ctx, (JSAtom)idx);
            } else {
                put_u32(bc_buf + pos + 1, idx);
#ifdef DUMP_READ_OBJECT
                bc_read_trace(s, "at %d, fixup atom: ", pos + 1);
                print_atom(s->ctx, idx);
                printf("\n");
#endif
            }
            break;
        default:
            break;
        }
        pos += len;
    }
    return 0;
}

static JSValue JS_ReadObjectRec_Ex(BCReaderState* s);

static JSValue JS_ReadFunctionTag_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSFunctionBytecode bc, *b;
    JSValue obj = JS_UNDEFINED;
    uint16_t v16;
    uint8_t v8;
    int idx, i, local_count;
    int function_size, cpool_offset, byte_code_offset;
    int closure_var_offset, vardefs_offset;

    memset(&bc, 0, sizeof(bc));
    bc.header.ref_count = 1;
    // bc.gc_header.mark = 0;

    if (bc_get_u16(s, &v16))
        goto fail;
    idx = 0;
    bc.has_prototype = bc_get_flags(v16, &idx, 1);
    bc.has_simple_parameter_list = bc_get_flags(v16, &idx, 1);
    bc.is_derived_class_constructor = bc_get_flags(v16, &idx, 1);
    bc.need_home_object = bc_get_flags(v16, &idx, 1);
    bc.func_kind = bc_get_flags(v16, &idx, 2);
    bc.new_target_allowed = bc_get_flags(v16, &idx, 1);
    bc.super_call_allowed = bc_get_flags(v16, &idx, 1);
    bc.super_allowed = bc_get_flags(v16, &idx, 1);
    bc.arguments_allowed = bc_get_flags(v16, &idx, 1);
    bc.has_debug = bc_get_flags(v16, &idx, 1);
    bc.backtrace_barrier = bc_get_flags(v16, &idx, 1);
    bc.read_only_bytecode = s->is_rom_data;
    if (bc_get_u8(s, &v8))
        goto fail;
    bc.js_mode = v8;
    if (bc_get_atom_ex(s, &bc.func_name)) //@ atom leak if failure
        goto fail;
    if (bc_get_leb128_u16(s, &bc.arg_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.var_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.defined_arg_count))
        goto fail;
    if (bc_get_leb128_u16(s, &bc.stack_size))
        goto fail;
    if (bc_get_leb128_int(s, &bc.closure_var_count))
        goto fail;
    if (bc_get_leb128_int(s, &bc.cpool_count))
        goto fail;
    if (bc_get_leb128_int(s, &bc.byte_code_len))
        goto fail;
    if (bc_get_leb128_int(s, &local_count))
        goto fail;

    if (bc.has_debug) {
        function_size = sizeof(*b);
    } else {
        function_size = offsetof(JSFunctionBytecode, debug);
    }
    cpool_offset = function_size;
    function_size += bc.cpool_count * sizeof(*bc.cpool);
    vardefs_offset = function_size;
    function_size += local_count * sizeof(*bc.vardefs);
    closure_var_offset = function_size;
    function_size += bc.closure_var_count * sizeof(*bc.closure_var);
    byte_code_offset = function_size;
    if (!bc.read_only_bytecode) {
        function_size += bc.byte_code_len;
    }

    b = js_mallocz(ctx, function_size);
    if (!b)
        return JS_EXCEPTION;

    memcpy(b, &bc, offsetof(JSFunctionBytecode, debug));
    b->header.ref_count = 1;
    if (local_count != 0) {
        b->vardefs = (void*)((uint8_t*)b + vardefs_offset);
    }
    if (b->closure_var_count != 0) {
        b->closure_var = (void*)((uint8_t*)b + closure_var_offset);
    }
    if (b->cpool_count != 0) {
        b->cpool = (void*)((uint8_t*)b + cpool_offset);
    }

    add_gc_object(ctx->rt, &b->header, JS_GC_OBJ_TYPE_FUNCTION_BYTECODE);

    obj = JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b);

#ifdef DUMP_READ_OBJECT
    bc_read_trace(s, "name: ");
    print_atom(s->ctx, b->func_name);
    printf("\n");
#endif
    bc_read_trace(s, "args=%d vars=%d defargs=%d closures=%d cpool=%d\n",
        b->arg_count, b->var_count, b->defined_arg_count,
        b->closure_var_count, b->cpool_count);
    bc_read_trace(s, "stack=%d bclen=%d locals=%d\n", b->stack_size,
        b->byte_code_len, local_count);

    if (local_count != 0) {
        bc_read_trace(s, "vars {\n");
        for (i = 0; i < local_count; i++) {
            JSVarDef* vd = &b->vardefs[i];
            if (bc_get_atom_ex(s, &vd->var_name))
                goto fail;
            if (bc_get_leb128_int(s, &vd->scope_level))
                goto fail;
            if (bc_get_leb128_int(s, &vd->scope_next))
                goto fail;
            vd->scope_next--;
            if (bc_get_u8(s, &v8))
                goto fail;
            idx = 0;
            vd->var_kind = bc_get_flags(v8, &idx, 4);
            vd->is_const = bc_get_flags(v8, &idx, 1);
            vd->is_lexical = bc_get_flags(v8, &idx, 1);
            vd->is_captured = bc_get_flags(v8, &idx, 1);
#ifdef DUMP_READ_OBJECT
            bc_read_trace(s, "name: ");
            print_atom(s->ctx, vd->var_name);
            printf("\n");
#endif
        }
        bc_read_trace(s, "}\n");
    }
    if (b->closure_var_count != 0) {
        bc_read_trace(s, "closure vars {\n");
        for (i = 0; i < b->closure_var_count; i++) {
            JSClosureVar* cv = &b->closure_var[i];
            int var_idx;
            if (bc_get_atom_ex(s, &cv->var_name))
                goto fail;
            if (bc_get_leb128_int(s, &var_idx))
                goto fail;
            cv->var_idx = var_idx;
            if (bc_get_u8(s, &v8))
                goto fail;
            idx = 0;
            cv->is_local = bc_get_flags(v8, &idx, 1);
            cv->is_arg = bc_get_flags(v8, &idx, 1);
            cv->is_const = bc_get_flags(v8, &idx, 1);
            cv->is_lexical = bc_get_flags(v8, &idx, 1);
            cv->var_kind = bc_get_flags(v8, &idx, 4);
#ifdef DUMP_READ_OBJECT
            bc_read_trace(s, "name: ");
            print_atom(s->ctx, cv->var_name);
            printf("\n");
#endif
        }
        bc_read_trace(s, "}\n");
    }
    {
        bc_read_trace(s, "bytecode {\n");
        if (JS_ReadFunctionBytecode_Ex(s, b, byte_code_offset, b->byte_code_len))
            goto fail;
        bc_read_trace(s, "}\n");
    }
    if (b->has_debug) {
        /* read optional debug information */
        bc_read_trace(s, "debug {\n");
        if (bc_get_atom_ex(s, &b->debug.filename))
            goto fail;
        if (bc_get_leb128_int(s, &b->debug.line_num))
            goto fail;
        if (bc_get_leb128_int(s, &b->debug.pc2line_len))
            goto fail;
        if (b->debug.pc2line_len) {
            b->debug.pc2line_buf = js_mallocz(ctx, b->debug.pc2line_len);
            if (!b->debug.pc2line_buf)
                goto fail;
            if (bc_get_buf(s, b->debug.pc2line_buf, b->debug.pc2line_len))
                goto fail;
        }
#ifdef DUMP_READ_OBJECT
        bc_read_trace(s, "filename: ");
        print_atom(s->ctx, b->debug.filename);
        printf("\n");
#endif
        bc_read_trace(s, "}\n");
    }
    if (b->cpool_count != 0) {
        bc_read_trace(s, "cpool {\n");
        for (i = 0; i < b->cpool_count; i++) {
            JSValue val;
            val = JS_ReadObjectRec_Ex(s);
            if (JS_IsException(val))
                goto fail;
            b->cpool[i] = val;
        }
        bc_read_trace(s, "}\n");
    }
    b->realm = JS_DupContext(ctx);
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue JS_ReadModule_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSValue obj;
    JSModuleDef* m = NULL;
    JSAtom module_name;
    int i;
    uint8_t v8;

    if (bc_get_atom_ex(s, &module_name))
        goto fail;
#ifdef DUMP_READ_OBJECT
    bc_read_trace(s, "name: ");
    print_atom(s->ctx, module_name);
    printf("\n");
#endif
    m = js_new_module_def(ctx, module_name);
    if (!m)
        goto fail;
    obj = JS_DupValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
    if (bc_get_leb128_int(s, &m->req_module_entries_count))
        goto fail;
    if (m->req_module_entries_count != 0) {
        m->req_module_entries_size = m->req_module_entries_count;
        m->req_module_entries = js_mallocz(ctx, sizeof(m->req_module_entries[0]) * m->req_module_entries_size);
        if (!m->req_module_entries)
            goto fail;
        for (i = 0; i < m->req_module_entries_count; i++) {
            JSReqModuleEntry* rme = &m->req_module_entries[i];
            if (bc_get_atom_ex(s, &rme->module_name))
                goto fail;
        }
    }

    if (bc_get_leb128_int(s, &m->export_entries_count))
        goto fail;
    if (m->export_entries_count != 0) {
        m->export_entries_size = m->export_entries_count;
        m->export_entries = js_mallocz(ctx, sizeof(m->export_entries[0]) * m->export_entries_size);
        if (!m->export_entries)
            goto fail;
        for (i = 0; i < m->export_entries_count; i++) {
            JSExportEntry* me = &m->export_entries[i];
            if (bc_get_u8(s, &v8))
                goto fail;
            me->export_type = v8;
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                if (bc_get_leb128_int(s, &me->u.local.var_idx))
                    goto fail;
            } else {
                if (bc_get_leb128_int(s, &me->u.req_module_idx))
                    goto fail;
                if (bc_get_atom_ex(s, &me->local_name))
                    goto fail;
            }
            if (bc_get_atom_ex(s, &me->export_name))
                goto fail;
        }
    }

    if (bc_get_leb128_int(s, &m->star_export_entries_count))
        goto fail;
    if (m->star_export_entries_count != 0) {
        m->star_export_entries_size = m->star_export_entries_count;
        m->star_export_entries = js_mallocz(ctx, sizeof(m->star_export_entries[0]) * m->star_export_entries_size);
        if (!m->star_export_entries)
            goto fail;
        for (i = 0; i < m->star_export_entries_count; i++) {
            JSStarExportEntry* se = &m->star_export_entries[i];
            if (bc_get_leb128_int(s, &se->req_module_idx))
                goto fail;
        }
    }

    if (bc_get_leb128_int(s, &m->import_entries_count))
        goto fail;
    if (m->import_entries_count != 0) {
        m->import_entries_size = m->import_entries_count;
        m->import_entries = js_mallocz(ctx, sizeof(m->import_entries[0]) * m->import_entries_size);
        if (!m->import_entries)
            goto fail;
        for (i = 0; i < m->import_entries_count; i++) {
            JSImportEntry* mi = &m->import_entries[i];
            if (bc_get_leb128_int(s, &mi->var_idx))
                goto fail;
            if (bc_get_atom_ex(s, &mi->import_name))
                goto fail;
            if (bc_get_leb128_int(s, &mi->req_module_idx))
                goto fail;
        }
    }

    m->func_obj = JS_ReadObjectRec_Ex(s);
    if (JS_IsException(m->func_obj))
        goto fail;
    return obj;
fail:
    if (m) {
        js_free_module_def(ctx, m);
    }
    return JS_EXCEPTION;
}

static JSValue JS_ReadObjectTag_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSValue obj;
    uint32_t prop_count, i;
    JSAtom atom;
    JSValue val;
    int ret;

    obj = JS_NewObject(ctx);
    if (BC_add_object_ref(s, obj))
        goto fail;
    if (bc_get_leb128(s, &prop_count))
        goto fail;
    for (i = 0; i < prop_count; i++) {
        if (bc_get_atom_ex(s, &atom))
            goto fail;
#ifdef DUMP_READ_OBJECT
        bc_read_trace(s, "propname: ");
        print_atom(s->ctx, atom);
        printf("\n");
#endif
        val = JS_ReadObjectRec(s);
        if (JS_IsException(val)) {
            JS_FreeAtom(ctx, atom);
            goto fail;
        }
        ret = JS_DefinePropertyValue(ctx, obj, atom, val, JS_PROP_C_W_E);
        JS_FreeAtom(ctx, atom);
        if (ret < 0)
            goto fail;
    }
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue JS_ReadArray_Ex(BCReaderState* s, int tag)
{
    JSContext* ctx = s->ctx;
    JSValue obj;
    uint32_t len, i;
    JSValue val;
    int ret, prop_flags;
    BOOL is_template;

    obj = JS_NewArray(ctx);
    if (BC_add_object_ref(s, obj))
        goto fail;
    is_template = (tag == BC_TAG_TEMPLATE_OBJECT);
    if (bc_get_leb128(s, &len))
        goto fail;
    for (i = 0; i < len; i++) {
        val = JS_ReadObjectRec_Ex(s);
        if (JS_IsException(val))
            goto fail;
        if (is_template)
            prop_flags = JS_PROP_ENUMERABLE;
        else
            prop_flags = JS_PROP_C_W_E;
        ret = JS_DefinePropertyValueUint32(ctx, obj, i, val, prop_flags);
        if (ret < 0)
            goto fail;
    }
    if (is_template) {
        val = JS_ReadObjectRec_Ex(s);
        if (JS_IsException(val))
            goto fail;
        if (!JS_IsUndefined(val)) {
            ret = JS_DefinePropertyValue(ctx, obj, JS_ATOM_raw, val, 0);
            if (ret < 0)
                goto fail;
        }
        JS_PreventExtensions(ctx, obj);
    }
    return obj;
fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue JS_ReadTypedArray_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSValue obj = JS_UNDEFINED, array_buffer = JS_UNDEFINED;
    uint8_t array_tag;
    JSValueConst args[3];
    uint32_t offset, len, idx;

    if (bc_get_u8(s, &array_tag))
        return JS_EXCEPTION;
    if (array_tag >= JS_TYPED_ARRAY_COUNT)
        return JS_ThrowTypeError(ctx, "invalid typed array");
    if (bc_get_leb128(s, &len))
        return JS_EXCEPTION;
    if (bc_get_leb128(s, &offset))
        return JS_EXCEPTION;
    /* XXX: this hack could be avoided if the typed array could be
     created before the array buffer */
    idx = s->objects_count;
    if (BC_add_object_ref1(s, NULL))
        goto fail;
    array_buffer = JS_ReadObjectRec_Ex(s);
    if (JS_IsException(array_buffer))
        return JS_EXCEPTION;
    if (!js_get_array_buffer(ctx, array_buffer)) {
        JS_FreeValue(ctx, array_buffer);
        return JS_EXCEPTION;
    }
    args[0] = array_buffer;
    args[1] = JS_NewInt64(ctx, offset);
    args[2] = JS_NewInt64(ctx, len);
    obj = js_typed_array_constructor(ctx, JS_UNDEFINED, 3, args,
        JS_CLASS_UINT8C_ARRAY + array_tag);
    if (JS_IsException(obj))
        goto fail;
    if (s->allow_reference) {
        s->objects[idx] = JS_VALUE_GET_OBJ(obj);
    }
    JS_FreeValue(ctx, array_buffer);
    return obj;
fail:
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue JS_ReadDate_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSValue val, obj = JS_UNDEFINED;

    val = JS_ReadObjectRec_Ex(s);
    if (JS_IsException(val))
        goto fail;
    if (!JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Number tag expected for date");
        goto fail;
    }
    obj = JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_DATE],
        JS_CLASS_DATE);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    JS_SetObjectData(ctx, obj, val);
    return obj;
fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue JS_ReadObjectValue_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    JSValue val, obj = JS_UNDEFINED;

    val = JS_ReadObjectRec_Ex(s);
    if (JS_IsException(val))
        goto fail;
    obj = JS_ToObject(ctx, val);
    if (JS_IsException(obj))
        goto fail;
    if (BC_add_object_ref(s, obj))
        goto fail;
    JS_FreeValue(ctx, val);
    return obj;
fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}
static JSValue JS_ReadObjectRec_Ex(BCReaderState* s)
{
    JSContext* ctx = s->ctx;
    uint8_t tag;
    JSValue obj = JS_UNDEFINED;

    if (js_check_stack_overflow(ctx->rt, 0))
        return JS_ThrowStackOverflow(ctx);

    if (bc_get_u8(s, &tag))
        return JS_EXCEPTION;

    bc_read_trace(s, "%s {\n", bc_tag_str[tag]);

    switch (tag) {
    case BC_TAG_NULL:
        obj = JS_NULL;
        break;
    case BC_TAG_UNDEFINED:
        obj = JS_UNDEFINED;
        break;
    case BC_TAG_BOOL_FALSE:
    case BC_TAG_BOOL_TRUE:
        obj = JS_NewBool(ctx, tag - BC_TAG_BOOL_FALSE);
        break;
    case BC_TAG_INT32: {
        int32_t val;
        if (bc_get_sleb128(s, &val))
            return JS_EXCEPTION;
        bc_read_trace(s, "%d\n", val);
        obj = JS_NewInt32(ctx, val);
    } break;
    case BC_TAG_FLOAT64: {
        JSFloat64Union u;
        if (bc_get_u64(s, &u.u64))
            return JS_EXCEPTION;
        bc_read_trace(s, "%g\n", u.d);
        obj = __JS_NewFloat64(ctx, u.d);
    } break;
    case BC_TAG_STRING: {
        JSString* p;
        p = JS_ReadString(s);
        if (!p)
            return JS_EXCEPTION;
        obj = JS_MKPTR(JS_TAG_STRING, p);
    } break;
    case BC_TAG_FUNCTION_BYTECODE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        obj = JS_ReadFunctionTag_Ex(s);
        break;
    case BC_TAG_MODULE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        obj = JS_ReadModule_Ex(s);
        break;
    case BC_TAG_OBJECT:
        obj = JS_ReadObjectTag_Ex(s);
        break;
    case BC_TAG_ARRAY:
    case BC_TAG_TEMPLATE_OBJECT:
        obj = JS_ReadArray_Ex(s, tag);
        break;
    case BC_TAG_TYPED_ARRAY:
        obj = JS_ReadTypedArray_Ex(s);
        break;
    case BC_TAG_ARRAY_BUFFER:
        obj = JS_ReadArrayBuffer(s);
        break;
    case BC_TAG_SHARED_ARRAY_BUFFER:
        if (!s->allow_sab || !ctx->rt->sab_funcs.sab_dup)
            goto invalid_tag;
        obj = JS_ReadSharedArrayBuffer(s);
        break;
    case BC_TAG_DATE:
        obj = JS_ReadDate(s);
        break;
    case BC_TAG_OBJECT_VALUE:
        obj = JS_ReadObjectValue_Ex(s);
        break;
#ifdef CONFIG_BIGNUM
    case BC_TAG_BIG_INT:
    case BC_TAG_BIG_FLOAT:
    case BC_TAG_BIG_DECIMAL:
        obj = JS_ReadBigNum(s, tag);
        break;
#endif
    case BC_TAG_OBJECT_REFERENCE: {
        uint32_t val;
        if (!s->allow_reference)
            return JS_ThrowSyntaxError(ctx, "object references are not allowed");
        if (bc_get_leb128(s, &val))
            return JS_EXCEPTION;
        bc_read_trace(s, "%u\n", val);
        if (val >= s->objects_count) {
            return JS_ThrowSyntaxError(ctx, "invalid object reference (%u >= %u)",
                val, s->objects_count);
        }
        obj = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, s->objects[val]));
    } break;
    default:
    invalid_tag:
        return JS_ThrowSyntaxError(ctx, "invalid tag (tag=%d pos=%u)", tag,
            (unsigned int)(s->ptr - s->buf_start));
    }
    bc_read_trace(s, "}\n");
    return obj;
}

static int JS_ReadObjectAtoms_Ex(BCReaderState* s)
{
    uint8_t v8;

    if (bc_get_u8(s, &v8))
        return -1;
    /* XXX: could support byte swapped input */
    if (v8 != BC_VERSION) {
        JS_ThrowSyntaxError(s->ctx, "invalid version (%d expected=%d)", v8,
            BC_VERSION);
        return -1;
    }
    return 0;
}

JSValue JS_ReadObject_Ex(JSContext* ctx, const uint8_t* buf, size_t buf_len,
    int flags)
{
    BCReaderState ss, *s = &ss;
    JSValue obj;

    ctx->binary_object_count += 1;
    ctx->binary_object_size += buf_len;

    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    s->buf_start = buf;
    s->buf_end = buf + buf_len;
    s->ptr = buf;
    s->allow_bytecode = ((flags & JS_READ_OBJ_BYTECODE) != 0);
    s->is_rom_data = ((flags & JS_READ_OBJ_ROM_DATA) != 0);
    s->allow_sab = ((flags & JS_READ_OBJ_SAB) != 0);
    s->allow_reference = ((flags & JS_READ_OBJ_REFERENCE) != 0);
    if (s->allow_bytecode)
        s->first_atom = JS_ATOM_END;
    else
        s->first_atom = 1;
    if (JS_ReadObjectAtoms_Ex(s)) {
        obj = JS_EXCEPTION;
    } else {
        obj = JS_ReadObjectRec_Ex(s);
    }
    bc_reader_free(s);
    return obj;
}

static int bc_put_atom_ex(BCWriterState* s, JSAtom atom)
{
    uint32_t v;

    if (__JS_AtomIsTaggedInt(atom)) {
        v = (__JS_AtomToUInt32(atom) << 1) | 1;
    } else {
        v = atom;
        v <<= 1;
    }
    bc_put_leb128(s, v);
    return 0;
}

static int JS_WriteFunctionBytecode_Ex(BCWriterState* s, const uint8_t* bc_buf1,
    int bc_len)
{
    uint8_t* bc_buf;

    bc_buf = js_malloc(s->ctx, bc_len);
    if (!bc_buf)
        return -1;
    memcpy(bc_buf, bc_buf1, bc_len);

    if (s->byte_swap)
        bc_byte_swap(bc_buf, bc_len);

    dbuf_put(&s->dbuf, bc_buf, bc_len);

    js_free(s->ctx, bc_buf);
    return 0;
}

static int JS_WriteObjectRec_Ex(BCWriterState* s, JSValueConst obj);

static int JS_WriteFunctionTag_Ex(BCWriterState* s, JSValueConst obj)
{
    JSFunctionBytecode* b = JS_VALUE_GET_PTR(obj);
    uint32_t flags;
    int idx, i;

    bc_put_u8(s, BC_TAG_FUNCTION_BYTECODE);
    flags = idx = 0;
    bc_set_flags(&flags, &idx, b->has_prototype, 1);
    bc_set_flags(&flags, &idx, b->has_simple_parameter_list, 1);
    bc_set_flags(&flags, &idx, b->is_derived_class_constructor, 1);
    bc_set_flags(&flags, &idx, b->need_home_object, 1);
    bc_set_flags(&flags, &idx, b->func_kind, 2);
    bc_set_flags(&flags, &idx, b->new_target_allowed, 1);
    bc_set_flags(&flags, &idx, b->super_call_allowed, 1);
    bc_set_flags(&flags, &idx, b->super_allowed, 1);
    bc_set_flags(&flags, &idx, b->arguments_allowed, 1);
    bc_set_flags(&flags, &idx, b->has_debug, 1);
    bc_set_flags(&flags, &idx, b->backtrace_barrier, 1);
    assert(idx <= 16);
    bc_put_u16(s, flags);
    bc_put_u8(s, b->js_mode);
    bc_put_atom_ex(s, b->func_name);

    bc_put_leb128(s, b->arg_count);
    bc_put_leb128(s, b->var_count);
    bc_put_leb128(s, b->defined_arg_count);
    bc_put_leb128(s, b->stack_size);
    bc_put_leb128(s, b->closure_var_count);
    bc_put_leb128(s, b->cpool_count);
    bc_put_leb128(s, b->byte_code_len);
    if (b->vardefs) {
        /* XXX: this field is redundant */
        bc_put_leb128(s, b->arg_count + b->var_count);
        for (i = 0; i < b->arg_count + b->var_count; i++) {
            JSVarDef* vd = &b->vardefs[i];
            bc_put_atom_ex(s, vd->var_name);
            bc_put_leb128(s, vd->scope_level);
            bc_put_leb128(s, vd->scope_next + 1);
            flags = idx = 0;
            bc_set_flags(&flags, &idx, vd->var_kind, 4);
            bc_set_flags(&flags, &idx, vd->is_const, 1);
            bc_set_flags(&flags, &idx, vd->is_lexical, 1);
            bc_set_flags(&flags, &idx, vd->is_captured, 1);
            assert(idx <= 8);
            bc_put_u8(s, flags);
        }
    } else {
        bc_put_leb128(s, 0);
    }

    for (i = 0; i < b->closure_var_count; i++) {
        JSClosureVar* cv = &b->closure_var[i];
        bc_put_atom_ex(s, cv->var_name);
        bc_put_leb128(s, cv->var_idx);
        flags = idx = 0;
        bc_set_flags(&flags, &idx, cv->is_local, 1);
        bc_set_flags(&flags, &idx, cv->is_arg, 1);
        bc_set_flags(&flags, &idx, cv->is_const, 1);
        bc_set_flags(&flags, &idx, cv->is_lexical, 1);
        bc_set_flags(&flags, &idx, cv->var_kind, 4);
        assert(idx <= 8);
        bc_put_u8(s, flags);
    }

    if (JS_WriteFunctionBytecode_Ex(s, b->byte_code_buf, b->byte_code_len))
        goto fail;

    if (b->has_debug) {
        bc_put_atom_ex(s, b->debug.filename);
        bc_put_leb128(s, b->debug.line_num);
        bc_put_leb128(s, b->debug.pc2line_len);
        dbuf_put(&s->dbuf, b->debug.pc2line_buf, b->debug.pc2line_len);
    }

    for (i = 0; i < b->cpool_count; i++) {
        if (JS_WriteObjectRec_Ex(s, b->cpool[i]))
            goto fail;
    }
    return 0;
fail:
    return -1;
}

static int JS_WriteModule_Ex(BCWriterState* s, JSValueConst obj)
{
    JSModuleDef* m = JS_VALUE_GET_PTR(obj);
    int i;

    bc_put_u8(s, BC_TAG_MODULE);
    bc_put_atom_ex(s, m->module_name);

    bc_put_leb128(s, m->req_module_entries_count);
    for (i = 0; i < m->req_module_entries_count; i++) {
        JSReqModuleEntry* rme = &m->req_module_entries[i];
        bc_put_atom_ex(s, rme->module_name);
    }

    bc_put_leb128(s, m->export_entries_count);
    for (i = 0; i < m->export_entries_count; i++) {
        JSExportEntry* me = &m->export_entries[i];
        bc_put_u8(s, me->export_type);
        if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
            bc_put_leb128(s, me->u.local.var_idx);
        } else {
            bc_put_leb128(s, me->u.req_module_idx);
            bc_put_atom_ex(s, me->local_name);
        }
        bc_put_atom_ex(s, me->export_name);
    }

    bc_put_leb128(s, m->star_export_entries_count);
    for (i = 0; i < m->star_export_entries_count; i++) {
        JSStarExportEntry* se = &m->star_export_entries[i];
        bc_put_leb128(s, se->req_module_idx);
    }

    bc_put_leb128(s, m->import_entries_count);
    for (i = 0; i < m->import_entries_count; i++) {
        JSImportEntry* mi = &m->import_entries[i];
        bc_put_leb128(s, mi->var_idx);
        bc_put_atom_ex(s, mi->import_name);
        bc_put_leb128(s, mi->req_module_idx);
    }

    if (JS_WriteObjectRec_Ex(s, m->func_obj))
        goto fail;
    return 0;
fail:
    return -1;
}

static int JS_WriteArray_Ex(BCWriterState* s, JSValueConst obj)
{
    JSObject* p = JS_VALUE_GET_OBJ(obj);
    uint32_t i, len;
    JSValue val;
    int ret;
    BOOL is_template;

    if (s->allow_bytecode && !p->extensible) {
        /* not extensible array: we consider it is a
       template when we are saving bytecode */
        bc_put_u8(s, BC_TAG_TEMPLATE_OBJECT);
        is_template = TRUE;
    } else {
        bc_put_u8(s, BC_TAG_ARRAY);
        is_template = FALSE;
    }
    if (js_get_length32(s->ctx, &len, obj))
        goto fail1;
    bc_put_leb128(s, len);
    for (i = 0; i < len; i++) {
        val = JS_GetPropertyUint32(s->ctx, obj, i);
        if (JS_IsException(val))
            goto fail1;
        ret = JS_WriteObjectRec_Ex(s, val);
        JS_FreeValue(s->ctx, val);
        if (ret)
            goto fail1;
    }
    if (is_template) {
        val = JS_GetProperty(s->ctx, obj, JS_ATOM_raw);
        if (JS_IsException(val))
            goto fail1;
        ret = JS_WriteObjectRec_Ex(s, val);
        JS_FreeValue(s->ctx, val);
        if (ret)
            goto fail1;
    }
    return 0;
fail1:
    return -1;
}

static int JS_WriteObjectTag_Ex(BCWriterState* s, JSValueConst obj)
{
    JSObject* p = JS_VALUE_GET_OBJ(obj);
    uint32_t i, prop_count;
    JSShape* sh;
    JSShapeProperty* pr;
    int pass;
    JSAtom atom;

    bc_put_u8(s, BC_TAG_OBJECT);
    prop_count = 0;
    sh = p->shape;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1)
            bc_put_leb128(s, prop_count);
        for (i = 0, pr = get_shape_prop(sh); i < sh->prop_count; i++, pr++) {
            atom = pr->atom;
            if (atom != JS_ATOM_NULL && JS_AtomIsString(s->ctx, atom) && (pr->flags & JS_PROP_ENUMERABLE)) {
                if (pr->flags & JS_PROP_TMASK) {
                    JS_ThrowTypeError(s->ctx, "only value properties are supported");
                    goto fail;
                }
                if (pass == 0) {
                    prop_count++;
                } else {
                    bc_put_atom_ex(s, atom);
                    if (JS_WriteObjectRec_Ex(s, p->prop[i].u.value))
                        goto fail;
                }
            }
        }
    }
    return 0;
fail:
    return -1;
}

static int JS_WriteTypedArray_Ex(BCWriterState* s, JSValueConst obj)
{
    JSObject* p = JS_VALUE_GET_OBJ(obj);
    JSTypedArray* ta = p->u.typed_array;

    bc_put_u8(s, BC_TAG_TYPED_ARRAY);
    bc_put_u8(s, p->class_id - JS_CLASS_UINT8C_ARRAY);
    bc_put_leb128(s, p->u.array.count);
    bc_put_leb128(s, ta->offset);
    if (JS_WriteObjectRec_Ex(s, JS_MKPTR(JS_TAG_OBJECT, ta->buffer)))
        return -1;
    return 0;
}

static int JS_WriteObjectRec_Ex(BCWriterState* s, JSValueConst obj)
{
    uint32_t tag;

    if (js_check_stack_overflow(s->ctx->rt, 0)) {
        JS_ThrowStackOverflow(s->ctx);
        return -1;
    }

    tag = JS_VALUE_GET_NORM_TAG(obj);
    switch (tag) {
    case JS_TAG_NULL:
        bc_put_u8(s, BC_TAG_NULL);
        break;
    case JS_TAG_UNDEFINED:
        bc_put_u8(s, BC_TAG_UNDEFINED);
        break;
    case JS_TAG_BOOL:
        bc_put_u8(s, BC_TAG_BOOL_FALSE + JS_VALUE_GET_INT(obj));
        break;
    case JS_TAG_INT:
        bc_put_u8(s, BC_TAG_INT32);
        bc_put_sleb128(s, JS_VALUE_GET_INT(obj));
        break;
    case JS_TAG_FLOAT64: {
        JSFloat64Union u;
        bc_put_u8(s, BC_TAG_FLOAT64);
        u.d = JS_VALUE_GET_FLOAT64(obj);
        bc_put_u64(s, u.u64);
    } break;
    case JS_TAG_STRING: {
        JSString* p = JS_VALUE_GET_STRING(obj);
        bc_put_u8(s, BC_TAG_STRING);
        JS_WriteString(s, p);
    } break;
    case JS_TAG_FUNCTION_BYTECODE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        if (JS_WriteFunctionTag_Ex(s, obj))
            goto fail;
        break;
    case JS_TAG_MODULE:
        if (!s->allow_bytecode)
            goto invalid_tag;
        if (JS_WriteModule_Ex(s, obj))
            goto fail;
        break;
    case JS_TAG_OBJECT: {
        JSObject* p = JS_VALUE_GET_OBJ(obj);
        int ret, idx;

        if (s->allow_reference) {
            idx = js_object_list_find(s->ctx, &s->object_list, p);
            if (idx >= 0) {
                bc_put_u8(s, BC_TAG_OBJECT_REFERENCE);
                bc_put_leb128(s, idx);
                break;
            } else {
                if (js_object_list_add(s->ctx, &s->object_list, p))
                    goto fail;
            }
        } else {
            if (p->tmp_mark) {
                JS_ThrowTypeError(s->ctx, "circular reference");
                goto fail;
            }
            p->tmp_mark = 1;
        }
        switch (p->class_id) {
        case JS_CLASS_ARRAY:
            ret = JS_WriteArray_Ex(s, obj);
            break;
        case JS_CLASS_OBJECT:
            ret = JS_WriteObjectTag_Ex(s, obj);
            break;
        case JS_CLASS_ARRAY_BUFFER:
            ret = JS_WriteArrayBuffer(s, obj);
            break;
        case JS_CLASS_SHARED_ARRAY_BUFFER:
            if (!s->allow_sab)
                goto invalid_tag;
            ret = JS_WriteSharedArrayBuffer(s, obj);
            break;
        case JS_CLASS_DATE:
            bc_put_u8(s, BC_TAG_DATE);
            ret = JS_WriteObjectRec_Ex(s, p->u.object_data);
            break;
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
#ifdef CONFIG_BIGNUM
        case JS_CLASS_BIG_INT:
        case JS_CLASS_BIG_FLOAT:
        case JS_CLASS_BIG_DECIMAL:
#endif
            bc_put_u8(s, BC_TAG_OBJECT_VALUE);
            ret = JS_WriteObjectRec_Ex(s, p->u.object_data);
            break;
        default:
            if (p->class_id >= JS_CLASS_UINT8C_ARRAY && p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                ret = JS_WriteTypedArray_Ex(s, obj);
            } else {
                JS_ThrowTypeError(s->ctx, "unsupported object class");
                ret = -1;
            }
            break;
        }
        p->tmp_mark = 0;
        if (ret)
            goto fail;
    } break;
#ifdef CONFIG_BIGNUM
    case JS_TAG_BIG_INT:
    case JS_TAG_BIG_FLOAT:
    case JS_TAG_BIG_DECIMAL:
        if (JS_WriteBigNum(s, obj))
            goto fail;
        break;
#endif
    default:
    invalid_tag:
        JS_ThrowInternalError(s->ctx, "unsupported tag (%d)", tag);
        goto fail;
    }
    return 0;

fail:
    return -1;
}

static int JS_WriteObjectAtoms_Ex(BCWriterState* s)
{
    DynBuf dbuf1;
    int atoms_size;
    uint8_t version;

    dbuf1 = s->dbuf;
    js_dbuf_init(s->ctx, &s->dbuf);

    version = BC_VERSION;
    if (s->byte_swap)
        version ^= BC_BE_VERSION;
    bc_put_u8(s, version);

    /* XXX: should check for OOM in above phase */

    /* move the atoms at the start */
    /* XXX: could just append dbuf1 data, but it uses more memory if
     dbuf1 is larger than dbuf */
    atoms_size = s->dbuf.size;
    if (dbuf_realloc(&dbuf1, dbuf1.size + atoms_size))
        goto fail;
    memmove(dbuf1.buf + atoms_size, dbuf1.buf, dbuf1.size);
    memcpy(dbuf1.buf, s->dbuf.buf, atoms_size);
    dbuf1.size += atoms_size;
    dbuf_free(&s->dbuf);
    s->dbuf = dbuf1;
    return 0;
fail:
    dbuf_free(&dbuf1);
    return -1;
}

uint8_t* JS_WriteObject_Ex2(JSContext* ctx, size_t* psize, JSValueConst obj,
    int flags, uint8_t*** psab_tab,
    size_t* psab_tab_len)
{
    BCWriterState ss, *s = &ss;

    memset(s, 0, sizeof(*s));
    s->ctx = ctx;
    /* XXX: byte swapped output is untested */
    s->byte_swap = ((flags & JS_WRITE_OBJ_BSWAP) != 0);
    s->allow_bytecode = ((flags & JS_WRITE_OBJ_BYTECODE) != 0);
    s->allow_sab = ((flags & JS_WRITE_OBJ_SAB) != 0);
    s->allow_reference = ((flags & JS_WRITE_OBJ_REFERENCE) != 0);
    /* XXX: could use a different version when bytecode is included */
    if (s->allow_bytecode)
        s->first_atom = JS_ATOM_END;
    else
        s->first_atom = 1;
    js_dbuf_init(ctx, &s->dbuf);
    js_object_list_init(&s->object_list);

    if (JS_WriteObjectRec_Ex(s, obj))
        goto fail;
    if (JS_WriteObjectAtoms_Ex(s))
        goto fail;
    js_object_list_end(ctx, &s->object_list);
    js_free(ctx, s->atom_to_idx);
    js_free(ctx, s->idx_to_atom);
    *psize = s->dbuf.size;
    if (psab_tab)
        *psab_tab = s->sab_tab;
    if (psab_tab_len)
        *psab_tab_len = s->sab_tab_len;
    return s->dbuf.buf;
fail:
    js_object_list_end(ctx, &s->object_list);
    js_free(ctx, s->atom_to_idx);
    js_free(ctx, s->idx_to_atom);
    dbuf_free(&s->dbuf);
    *psize = 0;
    if (psab_tab)
        *psab_tab = NULL;
    if (psab_tab_len)
        *psab_tab_len = 0;
    return NULL;
}

uint8_t* JS_WriteObject_Ex(JSContext* ctx, size_t* psize, JSValueConst obj,
    int flags)
{
    return JS_WriteObject_Ex2(ctx, psize, obj, flags, NULL, NULL);
}

size_t get_data_len(JSRuntime* rt)
{
    size_t len = 0;
    len += 5 * sizeof(uint32_t);
    len += sizeof(uint32_t) * rt->atom_hash_size;
    len += sizeof(JSAtomStruct*) * rt->atom_size;
    len += sizeof(uint32_t);
    for (int32_t i = 0; i < rt->atom_size; i++) {
        JSAtomStruct* p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            int32_t size = sizeof(JSAtomStruct) + (p->len << p->is_wide_char) + 1 - p->is_wide_char;
            int32_t ceil_val = ceil((double)size / 4);
            len += ceil_val * 4 + sizeof(int32_t);
        }
    }
    return len;
}

char* save_atom_array(JSRuntime* rt, size_t* buf_len)
{
    int32_t len = get_data_len(rt);
    *buf_len = len;

    uint8_t* buf = js_mallocz_rt(rt, len + 1);
    if (buf == NULL) {
        return NULL;
    }

    int32_t pos = 0;
    *(int32_t*)(buf + pos) = rt->atom_hash_size;
    pos += sizeof(int32_t);
    *(int32_t*)(buf + pos) = rt->atom_size;
    pos += sizeof(int32_t);
    *(int32_t*)(buf + pos) = rt->atom_count;
    pos += sizeof(int32_t);
    *(int32_t*)(buf + pos) = rt->atom_count_resize;
    pos += sizeof(int32_t);
    *(int32_t*)(buf + pos) = rt->atom_free_index;
    pos += sizeof(int32_t);

    memcpy(buf + pos, rt->atom_hash, sizeof(uint32_t) * rt->atom_hash_size);
    pos += sizeof(uint32_t) * rt->atom_hash_size;

    memcpy(buf + pos, rt->atom_array, sizeof(JSAtomStruct*) * rt->atom_size);
    pos += sizeof(JSAtomStruct*) * rt->atom_size;

    // 保存字符串大小
    int32_t num = len - (pos) - sizeof(int32_t);
    *(int32_t*)(buf + pos) = num;
    pos += sizeof(int32_t);

    /// 考虑到指针地址低位不能位移，所以对每个jsstring做补齐操作
    char placeholder[4] = { '\0' };

    int32_t size = 0;
    int32_t placeholder_count = 0;
    for (int i = 0; i < rt->atom_size; i++) {
        JSAtomStruct* p = rt->atom_array[i];
        if (!atom_is_free(p)) {
            size = sizeof(JSAtomStruct) + (p->len << p->is_wide_char) + 1 - p->is_wide_char;
            placeholder_count = size % 4;
            // 保存字符串大小+sizeof(int32_t)
            *(int32_t*)(buf + pos) = ceil((double)size / 4) * 4 + sizeof(int32_t);
            pos += sizeof(int32_t);
            memcpy(buf + pos, p, size);
            // 设置引用计数为1
            ((JSAtomStruct*)(buf + pos))->header.ref_count = 1;
            pos += size;
            if (placeholder_count != 0) {
                memcpy(buf + pos, placeholder, 4 - placeholder_count);
                pos += (4 - placeholder_count);
            }
        }
    }

    return (char*)buf;
}
#endif
