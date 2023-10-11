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

#ifdef CONFIG_QUICKAPP_BYTECODE_OPTIMIZATION
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

    rt->atom_array = js_malloc_rt(rt, sizeof(JSAtomStruct*) * rt->atom_size);
    if (!rt->atom_array) {
        return;
    }

    uint32_t* atom_array = (uint32_t*)(buffer + pos);
    pos += sizeof(uint32_t) * rt->atom_size;

    int32_t jsstring_all_size = 0;
    jsstring_all_size = *(int32_t*)(buffer + pos);
    pos += sizeof(int32_t);

    rt->const_jsstring_buffer = js_malloc_rt(rt, jsstring_all_size);
    if (!rt->const_jsstring_buffer) {
        return;
    }
    memcpy(rt->const_jsstring_buffer, buffer + pos, jsstring_all_size);

    int32_t len = sizeof(int32_t);
    int32_t str_pos = 0;
    for (int i = 0; i < rt->atom_size; i++) {
        if (atom_array[i] == UINT32_MAX) {
            rt->atom_array[i] = (JSAtomStruct*)(rt->const_jsstring_buffer + str_pos + len);
            str_pos += *(int32_t*)(rt->const_jsstring_buffer + str_pos);
            rt->const_atom_count = i;
#ifdef DUMP_LEAKS
            list_add_tail(&rt->atom_array[i]->link, &rt->string_list);
#endif
        } else {
            rt->atom_array[i] = atom_set_free(atom_array[i]);
        }
    }
    rt->const_atom_count++;
    rt->atom_free_index = rt->const_atom_count;
}

static int JS_InitAtoms_Ex(JSRuntime* rt, const uint8_t* rt_str_info)
{
    load_atom_array(rt, rt_str_info);
    return 0;
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
    JS_FreeRuntime(rt);
    return NULL;
}

JSRuntime* JS_NewRuntime_Ex(const uint8_t* rt_str_info)
{
    return JS_NewRuntime_Ex2(&def_malloc_funcs, NULL, rt_str_info);
}
#endif

size_t get_data_len(JSRuntime* rt)
{
    size_t len = 0;
    len += 5 * sizeof(uint32_t);
    len += sizeof(uint32_t) * rt->atom_hash_size;
    len += sizeof(uint32_t) * rt->atom_size;
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

    uint32_t* atom_array = (uint32_t*)(buf + pos);
    for (int i = 0; i < rt->atom_size; i++) {
        JSAtomStruct* p = rt->atom_array[i];
        if (atom_is_free(p)) {
            // 存储下一个空闲位置信息
            atom_array[i] = atom_get_free(p);
        } else {
            // 标志这个位置有存储JSString
            atom_array[i] = UINT32_MAX;
        }
    }
    pos += sizeof(uint32_t) * rt->atom_size;

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
