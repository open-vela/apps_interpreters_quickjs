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

static int js_native_proxy_isArray(JSContext *ctx, JSValueConst obj)
{
  JSProxyData *s = JS_GetOpaque(obj, JS_CLASS_NATIVE_PROXY);
  if (!s)
    return FALSE;
  return JS_IsArray(ctx, s->target);
}

static JSValue js_native_proxy_getPrototypeOf(JSContext *ctx, JSValueConst obj)
{
  JSProxyData* s = JS_GetOpaque(obj, JS_CLASS_NATIVE_PROXY);
  if (!s)
    return JS_EXCEPTION;

  return JS_GetPrototype(ctx, s->target);
}

JS_BOOL JS_IsSameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2) {
   return js_same_value(ctx, op1, op2);
}

// The same with js_object_toString, only return tag.
JSValue JS_ObjectToStringTag(JSContext* ctx, JSValueConst this_val) {
    JSValue obj, tag;
    int is_array;
    JSAtom atom;
    JSObject *p;

    if (JS_IsNull(this_val)) {
        tag = JS_NewString(ctx, "Null");
    } else if (JS_IsUndefined(this_val)) {
        tag = JS_NewString(ctx, "Undefined");
    } else {
        obj = JS_ToObject(ctx, this_val);
        if (JS_IsException(obj))
            return obj;
        is_array = JS_IsArray(ctx, obj);
        if (is_array < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
        if (is_array) {
            atom = JS_ATOM_Array;
        } else if (JS_IsFunction(ctx, obj)) {
            atom = JS_ATOM_Function;
        } else {
            p = JS_VALUE_GET_OBJ(obj);
            switch(p->class_id) {
            case JS_CLASS_STRING:
            case JS_CLASS_ARGUMENTS:
            case JS_CLASS_MAPPED_ARGUMENTS:
            case JS_CLASS_ERROR:
            case JS_CLASS_BOOLEAN:
            case JS_CLASS_NUMBER:
            case JS_CLASS_DATE:
            case JS_CLASS_REGEXP:
                atom = ctx->rt->class_array[p->class_id].class_name;
                break;
            default:
                atom = JS_ATOM_Object;
                break;
            }
        }
        tag = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_toStringTag);
        JS_FreeValue(ctx, obj);
        if (JS_IsException(tag))
            return JS_EXCEPTION;
        if (!JS_IsString(tag)) {
            JS_FreeValue(ctx, tag);
            tag = JS_AtomToString(ctx, atom);
        }
    }
    return tag;
}

JS_BOOL JS_Atom_IsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom)
{
    return JS_AtomIsArrayIndex(ctx, pval, atom);
}

void JS_SetNativeProxyClassId(JSClassID class_id) {
  JS_CLASS_NATIVE_PROXY = class_id;
}
