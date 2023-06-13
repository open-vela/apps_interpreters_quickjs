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

#ifndef QUICKJS_JS_STRUCT_DEF_H
#define QUICKJS_JS_STRUCT_DEF_H

// enum 类型，不能使用该宏，不同编译器大小不一样
#define ACCESS_OBJECT_FIELD1(_heap_ctx_, _field_)                              \
  ACCESS_OBJECT_FIELD(_heap_ctx_, _field_, sizeof(_field_))

/**********************CONFIG_BIGNUM*******************************/
#define bf_t_access(_heap_ctx_, _field_)                                       \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->ctx, sizeof(void *));             \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->sign, sizeof(int));               \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->expn, sizeof(slimb_t));           \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->len, sizeof(limb_t));             \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->tab, sizeof(void *))

#define BFConstCache_access(_heap_ctx_, _field_)                               \
  bf_t_access(_heap_ctx_, &((_field_)->val));                                  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prec, sizeof(limb_t))

#define bf_context_t_access(_heap_ctx_, _field_)                               \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->realloc_opaque, sizeof(limb_t));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->realloc_func, sizeof(limb_t));    \
  BFConstCache_access(_heap_ctx_, &((_field_)->log2_cache));                   \
  BFConstCache_access(_heap_ctx_, &((_field_)->pi_cache));                     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->ntt_state, sizeof(void *));

#define JSNumericOperations_access(_heap_ctx_, _field_)                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->to_string, sizeof(void *) * 7)

/**********************JSOBJECT*******************************/
#define list_head_access(_heap_ctx_, _field_)                                  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prev, sizeof(void *));            \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->next, sizeof(void *))

#define JSGCObjectHeader_access(_heap_ctx_, _field_)                           \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->ref_count, sizeof(int));          \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->gc_obj_type, uint8_t);   \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->mark, uint8_t);          \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->dummy1, sizeof(uint8_t));         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->dummy2, sizeof(uint16_t));        \
  list_head_access(_heap_ctx_, &((_field_)->link));                            \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->id);

#define JSMallocFunctions_access(_heap_ctx_, _field_)                          \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->js_malloc, sizeof(void *) * 4);

#define JSMallocState_access(_heap_ctx_, _field_)                              \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->malloc_count, sizeof(size_t));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->malloc_size, sizeof(size_t));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->malloc_limit, sizeof(size_t));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->opaque, sizeof(void *))

#define JSSharedArrayBufferFunctions_access(_heap_ctx_, _field_)               \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->sab_alloc, sizeof(size_t) * 4)

#define JSRuntime_access(_heap_ctx_, _field_)                                  \
  JSMallocFunctions_access(_heap_ctx_, &((_field_)->mf));                      \
  JSMallocState_access(_heap_ctx_, &((_field_)->malloc_state));                \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->rt_info, sizeof(void *));         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_hash_size, sizeof(int));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_count, sizeof(int));         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_size, sizeof(int));          \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_count_resize, sizeof(int));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_hash, sizeof(void *));       \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_array, sizeof(void *));      \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom_free_index, sizeof(int));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->class_count, sizeof(int));        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->class_array, sizeof(void *));     \
  list_head_access(_heap_ctx_, &((_field_)->context_list));                    \
  list_head_access(_heap_ctx_, &((_field_)->gc_obj_list));                     \
  list_head_access(_heap_ctx_, &((_field_)->gc_zero_ref_count_list));          \
  list_head_access(_heap_ctx_, &((_field_)->tmp_obj_list));                    \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->gc_phase, uint32_t);     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->malloc_gc_threshold,              \
                      sizeof(size_t));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->stack_size, sizeof(uintptr_t));   \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->stack_top, sizeof(uintptr_t));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->stack_limit, sizeof(uintptr_t));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->current_exception,                \
                      sizeof(JSValue));                                        \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->in_out_of_memory,        \
                               uint32_t);                                      \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->current_stack_frame,              \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->interrupt_handler,                \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->interrupt_opaque,                 \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->host_promise_rejection_tracker,   \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_,                                              \
                      (_field_)->host_promise_rejection_tracker_opaque,        \
                      sizeof(void *));                                         \
  list_head_access(_heap_ctx_, &((_field_)->job_list));                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->module_normalize_func,            \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->module_loader_func,               \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->module_loader_opaque,             \
                      sizeof(void *));                                         \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->can_block, uint32_t);    \
  JSSharedArrayBufferFunctions_access(_heap_ctx_, &((_field_)->sab_funcs));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape_hash_bits, sizeof(int));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape_hash_size, sizeof(int));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape_hash_count, sizeof(int));   \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape_hash, sizeof(void *));      \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->user_opaque, sizeof(void *))

#define JSContext_access(_heap_ctx_, _field_)                                  \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->rt, sizeof(void *));              \
  list_head_access(_heap_ctx_, &((_field_)->link));                            \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->binary_object_count,              \
                      sizeof(uint16_t));                                       \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->binary_object_size, sizeof(int)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->array_shape, sizeof(void *));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->class_proto, sizeof(void *));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->function_proto, sizeof(JSValue)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->function_ctor, sizeof(JSValue));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->array_ctor, sizeof(JSValue));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->regexp_ctor, sizeof(JSValue));    \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->promise_ctor, sizeof(JSValue));   \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->native_error_proto,               \
                      sizeof(JSValue) * JS_NATIVE_ERROR_COUNT);                \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->iterator_proto, sizeof(JSValue)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->async_iterator_proto,             \
                      sizeof(JSValue));                                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->array_proto_values,               \
                      sizeof(JSValue));                                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->throw_type_error,                 \
                      sizeof(JSValue));                                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->eval_obj, sizeof(JSValue));       \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->global_obj, sizeof(JSValue));     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->global_var_obj, sizeof(JSValue)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->interrupt_counter, sizeof(int));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->is_error_property_enabled,        \
                      sizeof(BOOL));                                           \
  list_head_access(_heap_ctx_, &((_field_)->loaded_modules));                  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->compile_regexp, sizeof(void *));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->eval_internal, sizeof(void *));   \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->user_opaque, sizeof(void *))

#define JSObject_access(_heap_ctx_, _field_)                                   \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape, sizeof(void *));           \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prop, sizeof(void *));            \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->first_weak_ref, sizeof(void *));  \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->u.func, sizeof(void *) * 3)

#define JSShapeProperty_access(_heap_ctx_, _field_)                            \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->hash_next, uint32_t);    \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->flags, uint8_t);         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->atom, sizeof(JSAtom))

#define JSShape_access(_heap_ctx_, _field_)                                    \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->is_hashed, sizeof(uint8_t));      \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->has_small_array_index,            \
                      sizeof(uint8_t));                                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->hash, sizeof(uint32_t));          \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prop_hash_mask,                   \
                      sizeof(uint32_t));                                       \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prop_size, sizeof(int));          \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->prop_count, sizeof(int));         \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->deleted_prop_count, sizeof(int)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->shape_hash_next, sizeof(void *)); \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->proto, sizeof(void *))

#define JSVarDef_access(_heap_ctx_, _field_)                                   \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->var_name);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->scope_level);                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->scope_next);                     \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_const, uint8_t);      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_lexical, uint8_t);    \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_captured, uint8_t);   \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->var_kind, uint8_t);      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->func_pool_idx, int)

#define JSClosureVar_access(_heap_ctx_, _field_)                               \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_local, uint8_t);      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_arg, uint8_t);        \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_const, uint8_t);      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->is_lexical, uint8_t);    \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->var_kind, uint8_t);      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->var_idx);                        \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->var_name)

#define JSCPool_access(_heap_ctx_, _field_)                                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_));

#define JSFunctionBytecode_debug_access(_heap_ctx_, _field_)                   \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->filename);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->line_num);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->source_len);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->pc2line_len);                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->pc2line_buf);                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->source);

#define JSFunctionBytecode_access(_heap_ctx_, _field_)                         \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->js_mode);                        \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->has_prototype, uint8_t); \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_,                                     \
                               (_field_)->has_simple_parameter_list, uint8_t); \
  ACCESS_OBJECT_FIELD_BITFIELD(                                                \
      _heap_ctx_, (_field_)->is_derived_class_constructor, uint8_t);           \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->need_home_object,        \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->func_kind, uint8_t);     \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->new_target_allowed,      \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->super_call_allowed,      \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->super_allowed, uint8_t); \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->arguments_allowed,       \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->has_debug, uint8_t);     \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->backtrace_barrier,       \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->read_only_bytecode,      \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->byte_code_buf);                  \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->byte_code_len);                  \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->func_name);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->vardefs);                        \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->closure_var);                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->arg_count);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->var_count);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->defined_arg_count);              \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->stack_size);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->realm);                          \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->cpool);                          \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->cpool_count);                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->closure_var_count);              \
  JSFunctionBytecode_debug_access(_heap_ctx_, &(_field_)->debug)

#define JSStackFrame_access(_heap_ctx_, _field_)                               \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->prev_frame);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->cur_func);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->arg_buf);                        \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->var_buf);                        \
  list_head_access(_heap_ctx_, &(_field_)->var_ref_list);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->cur_pc);                         \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->arg_count);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->js_mode);                        \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->cur_sp)

#define JSAsyncFunctionState_access(_heap_ctx_, _field_)                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->this_val);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->argc);                           \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->throw_flag);                     \
  JSStackFrame_access(_heap_ctx_, &(_field_)->frame)

#define JSAsyncFunctionData_access(_heap_ctx_, _field_)                        \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->resolving_funcs,                  \
                      sizeof(JSValue) * 2);                                    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->is_active);                      \
  JSAsyncFunctionState_access(_heap_ctx_, &(_field_)->func_state)

#define JSRefCountHeader_access(_heap_ctx_, _field_)                           \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->ref_count);

#define JSExportEntry_access(_heap_ctx_, _field_)                              \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->u.local);                        \
  ACCESS_OBJECT_FIELD(_heap_ctx_, (_field_)->export_type, sizeof(uint8_t));    \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->local_name);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->export_name);

#define JSModuleDef_access(_heap_ctx_, _field_)                                \
  JSRefCountHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->module_name);                    \
  list_head_access(_heap_ctx_, &(_field_)->link);                              \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->req_module_entries);             \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->req_module_entries_count);       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->req_module_entries_size);        \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->export_entries);                 \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->export_entries_count);           \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->export_entries_size);            \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->star_export_entries);            \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->star_export_entries_count);      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->star_export_entries_size);       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->import_entries);                 \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->import_entries_count);           \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->import_entries_size);            \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->module_ns);                      \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->func_obj);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->init_func);                      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->resolved, uint8_t);      \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->func_created, uint8_t);  \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->instantiated, uint8_t);  \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->evaluated, uint8_t);     \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->eval_mark, uint8_t);     \
  ACCESS_OBJECT_FIELD_BITFIELD(_heap_ctx_, (_field_)->eval_has_exception,      \
                               uint8_t);                                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->eval_exception);                 \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->meta_obj);

#define JSVarRef_access(_heap_ctx_, _field_)                                   \
  JSGCObjectHeader_access(_heap_ctx_, &(_field_)->header);                     \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->pvalue);                         \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->value);

#define JSBoundFunction_access(_heap_ctx_, _field_)                            \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->func_obj);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->this_val);                       \
  ACCESS_OBJECT_FIELD1(_heap_ctx_, (_field_)->argc);

#endif // QUICKJS_JS_STRUCT_DEF_H
