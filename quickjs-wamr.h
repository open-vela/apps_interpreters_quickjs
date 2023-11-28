#ifndef QUICKJS_WAMR_H
#define QUICKJS_WAMR_H

#define JS_CLASS_OBJECT 1

enum {
    JS_TAG_EXT_OBJ = 8,
    JS_TAG_EXT_FUNC = 9,
    JS_TAG_EXT_INFC = 10
};

#ifndef QUICKJS_H
#include "quickjs.h"
#define JS_GetGlobalVar JS_GetGlobalVar_wrapper
#define JS_CallConstructorInternal JS_CallConstructorInternal_wrapper
#define find_atom find_atom_wrapper
#define set_array_length1 set_array_length_wrapper
#define JS_DefinePropertyDesc1 JS_DefinePropertyDesc_wrapper
#define js_operator_typeof1 js_operator_typeof_wrapper
#define JS_Dump1 JS_Dump_wrapper
#define JS_OrdinaryIsInstanceOf1 JS_OrdinaryIsInstanceOf_wrapper
#endif

/* Wrapper API added by ts2wasm to expose static APIs */
JSValue JS_GetGlobalVar_wrapper(JSContext *ctx, JSAtom prop, JS_BOOL throw_ref_error);
JSValue JS_CallConstructorInternal_wrapper(JSContext *ctx,
                                   JSValueConst func_obj,
                                   JSValueConst new_target,
                                   int argc, JSValue *argv,
                                   int flags);
JSAtom find_atom_wrapper(JSContext *ctx, const char *name);
int set_array_length_wrapper(JSContext *ctx, JSObject *p, JSValue val,
                      int flags);
int JS_DefinePropertyDesc_wrapper(JSContext *ctx, JSValueConst obj, JSAtom prop,
                           JSValueConst desc, int flags);
int js_operator_typeof_wrapper(JSContext *ctx, JSValueConst op1);
void JS_Dump_wrapper(JSRuntime *rt, JSValue *p);
int JS_DumpWithBuffer(JSRuntime *rt, JSValue *p, void *buffer, uint32_t len);

int JS_OrdinaryIsInstanceOf_wrapper(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj);
uint32_t getClassIdFromObject(JSObject *obj);
JSClassCall* getCallByClassId(JSRuntime *rt, uint32_t classId);

#endif