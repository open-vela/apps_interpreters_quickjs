#ifndef QUICKJS_MEMORY_DUMP_H
#define QUICKJS_MEMORY_DUMP_H
#include "quickjs.h"
#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define INVALID_MEMORY_PTR 0
#define CDP_SELF 0
#define CDP_CHILD 1

#define CDP_UNKNOW_DEFAULT_NAME ("anymouse")
#define CDP_OBJECT_DEFAULT_NAME ("object")
#define CDP_STRING_DEFAULT_NAME ("string")
#define CDP_BYTECODE_DEFAULT_NAME ("file")
#define CDP_SYMBOL_DEFAULT_NAME ("symbol")
#define CDP_VALUE_DEFAULT_NAME ("value")
#define CDP_VARREF_DEFAULT_NAME ("var_ref")
#define CDP_VARREF_NATIVE_FINCTION_NAME ("Native function")
#define CDP_UNDEFINED_NAME ("undefined")

typedef int64_t memory_object_id;
typedef int Self_or_child;

typedef enum CDPFreeString {
  CDP_FREE_NO = 0,
  CDP_FREE_YES = 1,
} CDPFreeString;

typedef struct CDP_memory_str_val {
  int flag;
  const char *name;
} CDP_memory_str_val;

typedef enum EntryType {
  EntryHidden = 0,        // Hidden node, may be filtered when shown to user.
  EntryArray = 1,         // An array of elements.
  EntryString = 2,        // A string.
  EntryObject = 3,        // A JS object (except for arrays and strings).
  EntryCode = 4,          // Compiled code.
  EntryClosure = 5,       // Function closure.
  EntryRegExp = 6,        // RegExp.
  EntryHeapNumber = 7,    // Number stored in the heap.
  EntryNative = 8,        // Native object (not from V8 heap).
  EntrySynthetic = 9,     // Synthetic object, usually used for grouping
                          // snapshot items together.
  EntryConsString = 10,   // Concatenated string. A pair of pointers to strings.
  EntrySlicedString = 11, // Sliced string. A fragment of another string.
  EntrySymbol = 12,       // A Symbol (ES6).
  EntryBigInt = 13,       // BigInt.
  EntryObjectShape = 14,  // Internal data used for tracking the shapes (or
                          // "hidden classes") of JS objects.
} EntryType;

typedef struct HeapUsage {
  int64_t totalSize;
  int64_t usedSize;
  int64_t totalCount;
} Macro_heap_info;

typedef enum GraphType {
  GraphContextVariable = 0, // A variable from a function context.
  kElement = 1,             // An element of an array.
  GraphProperty = 2,        // A named object property.
  GraphInternal = 3,        // A link that can't be accessed from JS,
                            // thus, its name isn't a real property name
                            // (e.g. parts of a ConsString).
  GraphHidden = 4,          // A link that is needed for proper sizes
                            // calculation, but may be hidden from user.
  GraphShortcut = 5,        // A link that must not be followed during
                            // sizes calculation.
  GraphWeak = 6             // A weak reference (ignored by the GC).
} GraphType;

typedef void (*CDP_add_memory_object)(JSRuntime *rt,
                                      memory_object_id parent_ptr,
                                      EntryType type, memory_object_id id,
                                      CDP_memory_str_val *val, int64_t size,
                                      CDP_memory_str_val *obj_name);

typedef void (*CDP_add_memory_object_child_by_id)(
    JSRuntime *rt, memory_object_id id, memory_object_id child,
    CDP_memory_str_val *child_name);
typedef void (*CDP_add_memory_object_size_by_id)(JSRuntime *rt,
                                                 memory_object_id id,
                                                 int64_t size);
typedef void (*CDP_add_memory_object_value_by_id)(JSRuntime *rt,
                                                  memory_object_id id,
                                                  CDP_memory_str_val *val);
typedef void (*CDP_add_memory_object_type_by_id)(JSRuntime *rt,
                                                 memory_object_id id,
                                                 EntryType type);
typedef void (*CDP_add_memory_object_obj_name_by_id)(
    JSRuntime *rt, memory_object_id id, CDP_memory_str_val *obj_name);

typedef void (*CDP_GC_obj_change)(JSRuntime *rt, memory_object_id id,
                                  int64_t count, int64_t size);

typedef struct DumpMemoryInfo {
  size_t is_started_memory_tracking;       /* Is heap object tracking enabled*/
  int is_memory_tracking_on_timer_started; /* Track memory along a timeline */
  CDP_add_memory_object add_memory_object;
  CDP_add_memory_object_child_by_id add_memory_object_child_by_id;
  CDP_add_memory_object_size_by_id add_memory_object_size_by_id;
  CDP_add_memory_object_value_by_id add_memory_object_value_by_id;
  CDP_add_memory_object_type_by_id add_memory_object_type_by_id;
  CDP_add_memory_object_obj_name_by_id add_memory_object_obj_name_by_id;
  CDP_GC_obj_change GC_obj_change;
} DumpMemoryInfo;

typedef struct Child_info {
  memory_object_id id;
  CDP_memory_str_val child_name;
} Child_info;
// Turn on engine memory tracking
void CDP_start_memory_tracking(JSRuntime *rt);
// Stop Engine Memory Tracking
void CDP_stop_memory_tracking(JSRuntime *rt);
// Get DumpMemoryInfo structure from rt
struct DumpMemoryInfo *getDumpMemoryInfo(JSRuntime *rt);
// Get the memory usage in the engine
Macro_heap_info CDP_get_heap_usage(JSRuntime *rt);
// Scan the memory objects in the engine and add them to the proxy tree
void CDP_sacn_heap_in_memory(JSRuntime *rt);
// Triggered when there are new objects in the engine
void CDP_get_stats_update_info(JSRuntime *rt, JSGCObjectHeader *h);
// Triggered when the memory object in the engine is released
void CDP_remove_gc_obj(JSRuntime *rt, JSGCObjectHeader *h);
// Create Memory Object Name
void CDP_create_obj_name(CDP_memory_str_val* name, const char *str);
// Get Memory Object Name
CDP_memory_str_val CDP_get_obj_name(JSRuntime *rt, JSAtom atom);
// Convert int to string
char *CDP_int_to_string(int num, char *str, int radix);
// Get the number and size of objects in the engine
void CDP_get_gc_obj_count_and_size(JSRuntime *rt, JSGCObjectHeader *gp,
                                   int64_t *count, int64_t *size);

int64_t getDumpMemoryId(void);
#ifdef __cplusplus
}
#endif
#endif
#endif