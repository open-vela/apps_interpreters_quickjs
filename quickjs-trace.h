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

#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE == 0
#define CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_DUP_SIZE 0
#endif

#if CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE > 0

typedef struct BackTrace {
  void *buffers[CONFIG_INTERPRETERS_QUICKJS_MEMORY_LEAK_TRACE_SIZE];
  int nptrs;
} BackTrace;

typedef struct JSBacktrace {
  BackTrace *backtrace;
  int size;
  int cnt;
  char *js_backtrace;
} JSBacktrace;

#endif

void JS_PrintStackFrame(JSRuntime *rt, JSGCObjectHeader *p);
void JS_FreeStackBacktrace(JSRuntime *rt, JSObject *obj);
void JS_Init_StackBacktrace(JSContext *ctx, JSObject *obj);
void JS_Record_Backtrace_Ctx(JSContext *ctx, JSValueConst obj);
void JS_Record_Backtrace_RT(JSRuntime *rt, JSValueConst obj);
