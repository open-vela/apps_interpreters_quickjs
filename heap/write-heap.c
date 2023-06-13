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

#include "data-buffer.h"
#include "js-struct-def.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define ACCESS_OBJECT_FIELD_BITFIELD(heap_ctx_, field_, type_)                 \
  _write_data_buffer_##type_(heap_ctx_, field_);
#define ACCESS_OBJECT_FIELD(heap_ctx_, field_, size_)                          \
  _write_data_buffer(heap_ctx_, &(field_), size_);

void _scan_jsvalue_string(heap_context *heap_ctx, JSValue arm_value) {
  uint32_t tag = JS_VALUE_GET_NORM_TAG(arm_value);
  switch (tag) {
  case JS_TAG_STRING: {
    JSString *str = JS_VALUE_GET_STRING(arm_value);
    if (!str->atom_type) {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, str,
                              sizeof(JSString) +
                                  (str->len << str->is_wide_char) + 1 -
                                  str->is_wide_char,
                              OBJECT_JS_STRING);
    }
    break;
  }
  }
}

void _scan_object_jsvalue(heap_context *heap_ctx, JSObject *pobj) {
  if (pobj == NULL)
    return;
  JSObject *p = pobj;
  int i = 0;
  switch (p->class_id) {
  case JS_CLASS_ARRAY:     /* u.array | length */
  case JS_CLASS_ARGUMENTS: /* u.array | length */
    if (p->fast_array && p->u.array.u.values) {
      FILL_PROTOCOL_DATA_ITEM(
          heap_ctx, p->u.array.u.values,
          sizeof(JSValue) * max_uint32(p->u.array.u1.size, p->u.array.count),
          OBJECT_DIRECT_COPY);
      for (int i = 0; i < p->u.array.count; i++) {
        _scan_jsvalue_string(heap_ctx, p->u.array.u.values[i]);
      }
    }
    break;
  case JS_CLASS_NUMBER:  /* u.object_data */
  case JS_CLASS_STRING:  /* u.object_data */
  case JS_CLASS_BOOLEAN: /* u.object_data */
  case JS_CLASS_SYMBOL:  /* u.object_data */
  case JS_CLASS_DATE:    /* u.object_data */
#ifdef CONFIG_BIGNUM
  case JS_CLASS_BIG_INT:     /* u.object_data */
  case JS_CLASS_BIG_FLOAT:   /* u.object_data */
  case JS_CLASS_BIG_DECIMAL: /* u.object_data */
#endif
    _scan_jsvalue_string(heap_ctx, p->u.object_data);
    break;
  case JS_CLASS_C_FUNCTION: /* u.cfunc */
    break;
  case JS_CLASS_BYTECODE_FUNCTION: /* u.func */
    break;
  case JS_CLASS_BOUND_FUNCTION: /* u.bound_function */
  {
    JSBoundFunction *bf = p->u.bound_function;
    if (bf) {
      p_protocol_data_item ppdi = _alloc_protocol_data_item_header(
          heap_ctx, sizeof(*bf) * 2 + sizeof(JSValue) * bf->argc);
      ppdi->arm_addr = (uint32_t)bf;
      ppdi->data_type = OBJECT_BOUND_FUNCTION;
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx);
      JSBoundFunction_access(heap_ctx, bf);
      if (bf->argc) {
        _write_data_buffer(heap_ctx, bf->argv, sizeof(JSValue) * bf->argc);
      }
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx) - ppdi->buffer_size;
      ppdi->shape_hash_size = 0;
      ppdi->shape_prop_size = 0;
      ppdi->object_type = 0;
      /* func_obj and this_val are objects */
      _scan_jsvalue_string(heap_ctx, bf->func_obj);
      _scan_jsvalue_string(heap_ctx, bf->this_val);
      for (i = 0; i < bf->argc; i++) {
        _scan_jsvalue_string(heap_ctx, bf->argv[i]);
      }
    }

  } break;
  case JS_CLASS_C_FUNCTION_DATA: /* u.c_function_data_record */
  {
    JSCFunctionDataRecord *fd = p->u.c_function_data_record;
    if (fd) {
      for (i = 0; i < fd->data_len; i++) {
        _scan_jsvalue_string(heap_ctx, fd->data[i]);
      }
    }
  } break;
  case JS_CLASS_REGEXP: /* u.regexp */
  {
    JSString *str = p->u.regexp.pattern;
    if (str && !str->atom_type) {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, str,
                              sizeof(JSString) +
                                  (str->len << str->is_wide_char) + 1 -
                                  str->is_wide_char,
                              OBJECT_JS_STRING);
    }
    str = p->u.regexp.bytecode;
    if (str && !str->atom_type) {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, str,
                              sizeof(JSString) +
                                  (str->len << str->is_wide_char) + 1 -
                                  str->is_wide_char,
                              OBJECT_JS_STRING);
    }
  } break;

  case JS_CLASS_FOR_IN_ITERATOR: /* u.for_in_iterator */
  {
    JSForInIterator *it = p->u.for_in_iterator;
    if (it) {
      _scan_jsvalue_string(heap_ctx, it->obj);
    }
  } break;
  case JS_CLASS_ARRAY_BUFFER:        /* u.array_buffer */
  case JS_CLASS_SHARED_ARRAY_BUFFER: /* u.array_buffer */
    break;
  case JS_CLASS_GENERATOR:    /* u.generator_data */
  case JS_CLASS_UINT8C_ARRAY: /* u.typed_array / u.array */
  case JS_CLASS_INT8_ARRAY:   /* u.typed_array / u.array */
  case JS_CLASS_UINT8_ARRAY:  /* u.typed_array / u.array */
  case JS_CLASS_INT16_ARRAY:  /* u.typed_array / u.array */
  case JS_CLASS_UINT16_ARRAY: /* u.typed_array / u.array */
  case JS_CLASS_INT32_ARRAY:  /* u.typed_array / u.array */
  case JS_CLASS_UINT32_ARRAY: /* u.typed_array / u.array */
#ifdef CONFIG_BIGNUM
  case JS_CLASS_BIG_INT64_ARRAY:  /* u.typed_array / u.array */
  case JS_CLASS_BIG_UINT64_ARRAY: /* u.typed_array / u.array */
#endif
  case JS_CLASS_FLOAT32_ARRAY: /* u.typed_array / u.array */
  case JS_CLASS_FLOAT64_ARRAY: /* u.typed_array / u.array */
  case JS_CLASS_DATAVIEW:      /* u.typed_array */
#ifdef CONFIG_BIGNUM
  case JS_CLASS_FLOAT_ENV: /* u.float_env */
#endif
  case JS_CLASS_MAP:                      /* u.map_state */
  case JS_CLASS_SET:                      /* u.map_state */
  case JS_CLASS_WEAKMAP:                  /* u.map_state */
  case JS_CLASS_WEAKSET:                  /* u.map_state */
  case JS_CLASS_MAP_ITERATOR:             /* u.map_iterator_data */
  case JS_CLASS_SET_ITERATOR:             /* u.map_iterator_data */
  case JS_CLASS_ARRAY_ITERATOR:           /* u.array_iterator_data */
  case JS_CLASS_STRING_ITERATOR:          /* u.array_iterator_data */
  case JS_CLASS_PROXY:                    /* u.proxy_data */
  case JS_CLASS_PROMISE:                  /* u.promise_data */
  case JS_CLASS_PROMISE_RESOLVE_FUNCTION: /* u.promise_function_data */
  case JS_CLASS_PROMISE_REJECT_FUNCTION:  /* u.promise_function_data */
  case JS_CLASS_ASYNC_FUNCTION_RESOLVE:   /* u.async_function_data */
  case JS_CLASS_ASYNC_FUNCTION_REJECT:    /* u.async_function_data */
  case JS_CLASS_ASYNC_FROM_SYNC_ITERATOR: /* u.async_from_sync_iterator_data */
  case JS_CLASS_ASYNC_GENERATOR:          /* u.async_generator_data */
                                          /* TODO */
  default:
    /* XXX: class definition should have an opaque block size */
    if (p->u.opaque) {
    }
    break;
  }
}

void _scan_gc_object_list(heap_context *heap_ctx, JSRuntime *rt,
                          struct list_head *gc_list) {
  struct list_head *el;
  list_for_each(el, gc_list) {
    JSGCObjectHeader *gp = list_entry(el, JSGCObjectHeader, link);
    // 不同的数据类型大小不一样需要特殊处理
    switch (gp->gc_obj_type) {
    case JS_GC_OBJ_TYPE_SHAPE: {
      // 此处需要特殊处理shape
      JSShape *sh = (JSShape *)gp;
      void *sh_alloc = get_alloc_from_shape(sh);
      uint32_t hash_size = sh->prop_hash_mask + 1;
      size_t size = get_shape_size(hash_size, sh->prop_size);
      p_protocol_data_item ppdi =
          _alloc_protocol_data_item_header(heap_ctx, size * 2);
      ppdi->arm_addr = (uint32_t)sh_alloc;
      ppdi->data_type = OBJECT_OBJECT_LIST;
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx);
      _write_data_buffer(heap_ctx, sh_alloc, hash_size * sizeof(uint32_t));
      JSShape_access(heap_ctx, sh);
      for (int idx = 0; idx < sh->prop_count; idx++) {
        JSShapeProperty_access(heap_ctx, &(sh->prop[idx]));
      }
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx) - ppdi->buffer_size;
      ppdi->shape_hash_size = hash_size;
      ppdi->shape_prop_size = sh->prop_size;
      ppdi->object_type = gp->gc_obj_type;
    } break;
    case JS_GC_OBJ_TYPE_JS_OBJECT: {
      JSObject *obj = (JSObject *)gp;
      {
        // 发送object
        FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(heap_ctx, obj, sizeof(JSObject),
                                          OBJECT_OBJECT_LIST, JSObject_access);
        ppdi->object_type = gp->gc_obj_type;

        if (obj->class_id == JS_CLASS_BYTECODE_FUNCTION &&
            obj->u.func.function_bytecode->closure_var_count != 0 &&
            obj->u.func.var_refs) {
          FILL_PROTOCOL_DATA_ITEM(
              heap_ctx, obj->u.func.var_refs,
              sizeof(JSVarRef *) *
                  obj->u.func.function_bytecode->closure_var_count,
              OBJECT_VAR_REF);
        }
      }
      {
        // 发送prop
        FILL_PROTOCOL_DATA_ITEM(heap_ctx, obj->prop,
                                sizeof(JSProperty) * obj->shape->prop_size,
                                OBJECT_OBJECT_LIST);
        ppdi->data_type = OBJECT_JS_PROPERTY;
        {
          JSShapeProperty *prs = get_shape_prop(obj->shape);
          for (int i = 0; i < obj->shape->prop_count; i++) {
            JSProperty *pr = &obj->prop[i];
            if (prs->atom != JS_ATOM_NULL) {
              if (prs->flags & JS_PROP_TMASK) {
              } else {
                _scan_jsvalue_string(heap_ctx, pr->u.value);
              }
            }
          }
        }
      }
      _scan_object_jsvalue(heap_ctx, obj);
    } break;
    case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE: {
      // 发送function
      JSFunctionBytecode *func = (JSFunctionBytecode *)gp;

      int func_size = 0;
      int ext_size = 0;
      func_size = sizeof(JSFunctionBytecode);
      // if (func->has_debug) {
      //     func_size = sizeof(JSFunctionBytecode);
      // } else {
      //     func_size = offsetof(JSFunctionBytecode, debug);
      // }
      ext_size += func->cpool_count * sizeof(*func->cpool);
      ext_size += (func->arg_count + func->var_count) * sizeof(*func->vardefs);
      ext_size += func->closure_var_count * sizeof(*func->closure_var);
      ext_size += func->byte_code_len;
      // if (!func->read_only_bytecode) {
      //     ext_size += func->byte_code_len;
      // }
      p_protocol_data_item ppdi =
          _alloc_protocol_data_item_header(heap_ctx, func_size * 2 + ext_size);
      ppdi->arm_addr = (uint32_t)func;
      ppdi->data_type = OBJECT_OBJECT_LIST;
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx);
      JSFunctionBytecode_access(heap_ctx, func);

      for (int i = 0; i < func->arg_count + func->var_count; i++) {
        JSVarDef_access(heap_ctx, &func->vardefs[i])
      }
      for (int i = 0; i < func->closure_var_count; i++) {
        JSClosureVar_access(heap_ctx, &func->closure_var[i])
      }
      // cpool
      for (int i = 0; i < func->cpool_count; i++) {
        JSCPool_access(heap_ctx, func->cpool[i])
      }
      // byte_code
      _write_data_buffer(heap_ctx, func->byte_code_buf, func->byte_code_len);
      ppdi->buffer_size = _get_data_buffer_offset(heap_ctx) - ppdi->buffer_size;
      ppdi->shape_hash_size = 0;
      ppdi->shape_prop_size = 0;
      ppdi->object_type = gp->gc_obj_type;
      {
        // cpool
        for (int i = 0; i < func->cpool_count; i++) {
          _scan_jsvalue_string(heap_ctx, func->cpool[i]);
        }
      }
      if (func->has_debug) {
        {
          FILL_PROTOCOL_DATA_ITEM(heap_ctx, func->debug.pc2line_buf,
                                  func->debug.pc2line_len, OBJECT_DIRECT_COPY);
        }
        {
          FILL_PROTOCOL_DATA_ITEM(heap_ctx, func->debug.source,
                                  func->debug.source_len, OBJECT_DIRECT_COPY);
        }
      }
      break;
    }
    case JS_GC_OBJ_TYPE_VAR_REF: {
      _scan_jsvalue_string(heap_ctx, ((JSVarRef *)gp)->value);
      FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(heap_ctx, (JSVarRef *)gp,
                                        sizeof(JSVarRef), OBJECT_OBJECT_LIST,
                                        JSVarRef_access);
      ppdi->object_type = gp->gc_obj_type;
      break;
    }
    case JS_GC_OBJ_TYPE_ASYNC_FUNCTION: {
      FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(
          heap_ctx, (JSAsyncFunctionData *)gp, sizeof(JSAsyncFunctionData),
          OBJECT_OBJECT_LIST, JSAsyncFunctionData_access);
      ppdi->object_type = gp->gc_obj_type;
      break;
    }
    case JS_GC_OBJ_TYPE_JS_CONTEXT: {
      {
        FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(heap_ctx, (JSContext *)gp,
                                          sizeof(JSContext), OBJECT_OBJECT_LIST,
                                          JSContext_access);
        ppdi->object_type = gp->gc_obj_type;
      }
      {
        JSContext *ctx = (JSContext *)gp;
        FILL_PROTOCOL_DATA_ITEM(heap_ctx, ctx->class_proto,
                                sizeof(ctx->class_proto[0]) * rt->class_count,
                                OBJECT_CLASS_PROTO);
      }
      {
        struct list_head *el;
        list_for_each(el, &((JSContext *)gp)->loaded_modules) {
          JSModuleDef *m = list_entry(el, JSModuleDef, link);
          FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(
              heap_ctx, (JSModuleDef *)m, sizeof(JSModuleDef), OBJECT_JS_MODULE,
              JSModuleDef_access);
          // print_atom((JSContext *)gp, m->module_name);
          // printf("\n");
          ppdi->object_type = JS_MODULE_DEF;
          if (m->export_entries_size > 0) {
            p_protocol_data_item ppdi = _alloc_protocol_data_item_header(
                heap_ctx, (m->export_entries_size * sizeof(JSExportEntry)) * 2);
            ppdi->arm_addr = (uint32_t)m->export_entries;
            ppdi->data_type = OBJECT_JS_MODULE;
            ppdi->buffer_size = _get_data_buffer_offset(heap_ctx);
            for (int idx = 0; idx < m->export_entries_size; idx++) {
              JSExportEntry_access(heap_ctx,
                                   (JSExportEntry *)&m->export_entries[idx]);
            }
            ppdi->buffer_size =
                _get_data_buffer_offset(heap_ctx) - ppdi->buffer_size;
            ppdi->shape_hash_size = m->export_entries_size;
            ppdi->shape_prop_size = 0;
            ppdi->object_type = JS_MODULE_EXPORT;
          }
        }
      }

    } break;
    default:
      break;
    }
  }
  return;
}

int _scan_rt_object(heap_context *heap_ctx, JSRuntime *rt) {
  int ret = 0;
  if (rt == NULL) {
    return ret;
  }

  for (int data_type_index = OBJECT_EMPTY; data_type_index < OBJECT_MAX;
       data_type_index++) {
    switch (data_type_index) {
    case OBJECT_RUNTIME: {
      FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(heap_ctx, rt, sizeof(JSRuntime),
                                        OBJECT_RUNTIME, JSRuntime_access);
    } break;
    case OBJECT_OBJECT_LIST: {
      _scan_gc_object_list(heap_ctx, rt, &rt->gc_obj_list);
    } break;
    case OBJECT_SHAPE_HASH: {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, rt->shape_hash,
                              sizeof(rt->shape_hash[0]) * rt->shape_hash_size,
                              data_type_index);
    } break;
    case OBJECT_ATOM_HASH: {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, rt->atom_hash,
                              sizeof(rt->atom_hash[0]) * rt->atom_hash_size,
                              data_type_index);
    } break;
    case OBJECT_ATOM_ARRAY: {
      FILL_PROTOCOL_DATA_ITEM(heap_ctx, rt->atom_array,
                              sizeof(rt->atom_array[0]) * rt->atom_size,
                              data_type_index);
    } break;
    case OBJECT_JS_STRING: {
      int str_len = 0;
      for (int i = 0; i < rt->atom_size; i++) {
        JSAtomStruct *p = rt->atom_array[i];
        if (!atom_is_free(p)) {
          if (p->len != 0) {
            str_len = (p->len << p->is_wide_char) + 1 - p->is_wide_char;
          } else {
            str_len = 0;
          }
          FILL_PROTOCOL_DATA_ITEM(heap_ctx, p, sizeof(JSString) + str_len,
                                  data_type_index);
        }
      }
    } break;
    default:
      break;
    }
  }
  _flush_data_buffer(heap_ctx);

  return ret;
}

#undef ACCESS_OBJECT_FIELD_BITFIELD
#undef ACCESS_OBJECT_FIELD

void dump_memory_usage(JSRuntime *rt, FILE *fp) {
  JSMemoryUsage stats;
  JS_ComputeMemoryUsage(rt, &stats);
  JS_DumpMemoryUsage(fp, &stats, rt);
}

int write_file_callback(void *fd, void *buffer, uint32_t len) {
  if (fd == NULL)
    return 0;
  return fwrite(buffer, len, 1, (FILE *)fd);
}

void write_heap_to_file(JSRuntime *rt) {
  char path[100];
  sprintf(path, "/data/quickjs-%ld.heap", time(NULL));
  FILE *fp = fopen(path, "ab");
  if (fp == NULL) {
    syslog(LOG_ERR, "==========Can't create output file=========== \n");
    return;
  }

  // 开始
  heap_context heap_ctx;
  memset(&heap_ctx, 0, sizeof(heap_context));
  _init_data_buffer(&heap_ctx, 4096);
  heap_ctx.fd = (void*)fp;
  heap_ctx.writer_callback = write_file_callback;
  _scan_rt_object(&heap_ctx, rt);
  _uninit_data_buffer(&heap_ctx);

  fclose(fp);

  dump_memory_usage(rt, stdout);
}
