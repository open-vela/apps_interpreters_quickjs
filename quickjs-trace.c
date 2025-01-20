/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE > 0
#include <execinfo.h>

static char *get_jsstacktrace(JSContext *ctx, JSValue exception) {
  // Convert the exception to a string representation
  JSValue message = JS_GetPropertyStr(ctx, exception, "stack");
  if (JS_IsException(message)) {
    return NULL; // If there's an error getting the stack, return it
  }

  // Convert the stack trace to a C string
  const char *stackStr = JS_ToCString(ctx, message);
  if (!stackStr) {
    JS_FreeValue(ctx, message);
    return NULL; // Handle memory allocation failure
  }

  // Create a new JS string to return
  char *jsStackTrace = js_strdup(ctx, stackStr);
  // Free the C string and the message value
  JS_FreeCString(ctx, stackStr);
  JS_FreeValue(ctx, message);

  return jsStackTrace;
}

static void print_jsstackframe(JSRuntime *rt, JSBacktrace *js_backtrace) {
  printf("\nJS stack frames:\n%s\n",
         js_backtrace->js_backtrace ? js_backtrace->js_backtrace : "");
}

static void js_stackbacktrace_init_rt(JSRuntime *rt, BackTrace *bt) {
  bt->nptrs = backtrace(bt->buffers,
                        CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE);
}

static void print_stackbacktrace(JSRuntime *rt, BackTrace *bt) {
  char **strings = backtrace_symbols(bt->buffers, bt->nptrs);
  if (strings != NULL) {
    printf("C stack frames:\n");
    for (int i = 0; i < bt->nptrs; i++) {
      printf("%s\n", strings[i]);
    }
  }
  js_free_rt(rt, strings);
}

static void js_record_backtrace(JSRuntime *rt, JSValueConst val) {
  if (!JS_IsObject(val))
    return;
  JSObject *obj = JS_VALUE_GET_OBJ(val);
  if (obj->class_id) {
    JSBacktrace *js_backtrace = &obj->backtrace;
    if (js_backtrace->backtrace == NULL)
      return;
    if (js_backtrace->cnt >= js_backtrace->size) {
      int size = js_backtrace->size * 2;
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_DUP_SIZE > 0
      if (rt->dup_size + size >=
          CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_DUP_SIZE) {
        return;
      }
      rt->dup_size += size;
#endif
      BackTrace *ptr =
          js_realloc_rt(rt, js_backtrace->backtrace, size * sizeof(BackTrace));
      if (ptr == NULL) {
        return;
      }
      js_backtrace->backtrace = ptr;
      js_backtrace->size = size;
    }
    BackTrace *bt = &(js_backtrace->backtrace[js_backtrace->cnt++]);
    js_stackbacktrace_init_rt(rt, bt);
  }
}

#endif

void JS_PrintStackFrame(JSRuntime *rt, JSGCObjectHeader *p) {
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE > 0
  if (p->gc_obj_type != JS_GC_OBJ_TYPE_JS_OBJECT) {
    return;
  }
  JSObject *obj = (JSObject *)p;
  JS_DumpGCObject(rt, p);
  JSBacktrace *js_backtrace = &obj->backtrace;
  print_jsstackframe(rt, js_backtrace);

  for (int i = 0; i < js_backtrace->cnt; ++i) {
    print_stackbacktrace(rt, js_backtrace->backtrace + i);
  }
#endif
}

void JS_FreeStackBacktrace(JSRuntime *rt, JSObject *obj) {
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE > 0
  JSBacktrace *js_backtrace = &obj->backtrace;
  js_free_rt(rt, js_backtrace->backtrace);
  js_free_rt(rt, js_backtrace->js_backtrace);
  js_backtrace->backtrace = NULL;
  js_backtrace->js_backtrace = NULL;
#endif
}

void JS_Init_StackBacktrace(JSContext *ctx, JSObject *obj) {
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE > 0
  JSValue stackFrame = JS_MKPTR(JS_TAG_OBJECT, obj);
  build_backtrace(ctx, stackFrame, NULL, 0, 0);
  JSBacktrace *js_backtrace = &obj->backtrace;
  js_backtrace->size = 1;
  js_backtrace->cnt = 0;
  js_backtrace->backtrace =
      js_malloc(ctx, sizeof(BackTrace) * js_backtrace->size);
  js_backtrace->js_backtrace = get_jsstacktrace(ctx, stackFrame);
  js_record_backtrace(ctx->rt, stackFrame);
  delete_property(ctx, obj, JS_ATOM_stack);
#endif
}

void JS_Record_Backtrace_Ctx(JSContext *ctx, JSValueConst val) {
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_DUP_SIZE > 0
  js_record_backtrace(ctx->rt, val);
#endif
}

void JS_Record_Backtrace_RT(JSRuntime *rt, JSValueConst val) {
#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_DUP_SIZE > 0
  js_record_backtrace(rt, val);
#endif
}