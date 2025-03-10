#ifndef QUICKJS_DEBUGGER_H
#define QUICKJS_DEBUGGER_H

#include "quickjs.h"

#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

//must keep update with definations in quickjs.c
typedef struct FunctionBytecodeDebugInfo {
    /* debug info, move to separate structure to save memory? */
    JSAtom filename;
    int line_num;
    int source_len;
    int pc2line_len;
    uint8_t *pc2line_buf;
    char *source;
} FunctionBytecodeDebugInfo;

#define PC2LINE_BASE     (-1)
#define PC2LINE_RANGE    5
#define PC2LINE_OP_FIRST 1
#define PC2LINE_DIFF_PC_MAX ((255 - PC2LINE_OP_FIRST) / PC2LINE_RANGE)


/////////////////////////////////////////////////////////////////////////////////


#define RESOLVE_RESULT_SUCCESS               0      /* breakpoint resolve success */
#define RESOLVE_RESULT_BREAKPOINTNOTFOUND   -1      /* could not find target breakpoint(by breakpointId) */
#define RESOLVE_RESULT_UNLOADED             -2      /* breakpoint target script not loaded */
#define RESOLVE_RESULT_HASHMISMATCH         -3      /* breakpoint target script hash mismatch */
#define RESOLVE_RESULT_MODUNEVALUATED       -4      /* breakpoint target module not evaluated */
#define RESOLVE_RESULT_PARAMERR             -5      /* breakpoint parameter error */
#define RESOLVE_RESULT_FAILED               -10     /* breakpoint resolve failed */

#define JS_DEBUGGER_EXCEPITON_STOP_NONE         0
#define JS_DEBUGGER_EXCEPTION_STOP_UNCAUGHT     1   /* quickjs do not have 'unhandled exception' now */
#define JS_DEBUGGER_EXCEPTION_STOP_ALL          2


#define JS_DEBUGGER_PAUSE_REASON_OTHER          0   /* breakpoint hit */
#define JS_DEBUGGER_PAUSE_REASON_EXCEPTION      1   /* for exception reason */

/**
 * @brief the Debugger.paused reason enum
 * 
 */
enum PauseReason {
    PAUSE_REASON_ambiguous,
    PAUSE_REASON_assert,
    PAUSE_REASON_CSPViolation,
    PAUSE_REASON_debugCommand,
    PAUSE_REASON_DOM,
    PAUSE_REASON_EventListener,
    PAUSE_REASON_exception,
    PAUSE_REASON_instrumentation,
    PAUSE_REASON_OOM,
    PAUSE_REASON_other,                     //hit breakpoint
    PAUSE_REASON_promiseRejection,
    PAUSE_REASON_XHR
};

typedef struct FunctionBytecodeDebuggerInfo {
    // same length as byte_code_buf.
    uint8_t *breakpoints;
    uint32_t dirty;
    int last_line_num;      //the function last line number
} FunctionBytecodeDebuggerInfo;

typedef struct JSDebuggerLocation {
    JSAtom filename;
    int line;
    int column;
} JSDebuggerLocation;

#define JS_DEBUGGER_STEP                1 << 0          //step next
#define JS_DEBUGGER_STEP_IN             1 << 1          //step into
#define JS_DEBUGGER_STEP_OUT            1 << 2          //step out
#define JS_DEBUGGER_STEP_CONTINUE       1 << 3          //resume


typedef struct JSDebuggerInfo JSDebuggerInfo;

typedef void(*update_loaded_script_entries)(JSContext* ctx, JSValue script_obj, const char* filename, const char *input, size_t input_len, int flags, uint32_t hash, int real_loaded);
typedef void(*check_resolve_breakpoints)(JSDebuggerInfo* info, const void* filename_or_module, int is_filename);
typedef void (*debugger_paused)(JSDebuggerInfo* info, int reason, const uint8_t *cur_pc);
typedef void (*debugger_check)(JSContext* ctx, JSDebuggerInfo* info, const uint8_t* cur_pc);

struct JSDebuggerInfo {
    
    JSContext *ctx;                     /* the original context to be debugged */
    JSContext *currCtx;                 /* current context */
    JSContext *debugging_ctx;           /* JSContext that is used to for the JSON transport and debugger state. */
    int is_error_throwing;              /* if in throwing exception process */
    int is_debugging;                   /* the debugging flag, to prevent js_debugger_check reentrent */
    int is_paused;                      /* the pause flag */
    int exception_stop_state;           /* see JS_DEBUGGER_EXCEPTION_STOP_* */
    int need_init_step;                 /* if step_over and step_depth need to be inited */
    int stepping;                       /* the stepping type, see JS_DEBUGGER_STEP_* */
    JSDebuggerLocation step_over;       /* the step operation start location */
    int step_depth;                     /* the step operation start stack depth */
    int  is_head_breakpoint;            /* Initial app.js head breakpoint */
    void* cdp_server;                   /* chrome devtools protocol server instance ptr */
    int wait_connection;                /* wait websocket client connect flag */
    int force_pause;                    /* force pause the script running */
    double start_time;                  /*cdp start timer */
    char * package_name;
    struct {
        debugger_check cb_debugger_check;
        update_loaded_script_entries cb_update_loaded_script_entries;
        check_resolve_breakpoints cb_check_resolve_breakpoints;
        debugger_paused cb_debugger_paused;
    } callbacks;
};

void js_debugger_check(JSContext *ctx, const uint8_t *pc);
void js_debugger_free(JSRuntime *rt, JSDebuggerInfo *info);

// begin internal api functions
// these functions all require access to quickjs internal structures.

JSDebuggerInfo *js_debugger_info(JSRuntime *rt);

// evaluates an expression at any stack frame. JS_Evaluate* only evaluates at the top frame.
JSValue js_debugger_evaluate(JSContext *ctx, int stack_index, JSValue expression);
////////////////////////////////////////////


#ifndef ONLY_CONTAINS_DEBUGGERINFO_DECLARE

//the private header info used for debugger
typedef struct JSStackFrame JSStackFrame;
typedef struct JSFunctionBytecode JSFunctionBytecode;

typedef enum {
    /* XXX: add more variable kinds here instead of using bit fields */
    JS_VAR_NORMAL,
    JS_VAR_FUNCTION_DECL, /* lexical var with function declaration */
    JS_VAR_NEW_FUNCTION_DECL, /* lexical var with async/generator
                                 function declaration */
    JS_VAR_CATCH,
    JS_VAR_FUNCTION_NAME, /* function expression name */
    JS_VAR_PRIVATE_FIELD,
    JS_VAR_PRIVATE_METHOD,
    JS_VAR_PRIVATE_GETTER,
    JS_VAR_PRIVATE_SETTER, /* must come after JS_VAR_PRIVATE_GETTER */
    JS_VAR_PRIVATE_GETTER_SETTER, /* must come after JS_VAR_PRIVATE_SETTER */
} JSVarKindEnum;


/* XXX: could use a different structure in bytecode functions to save
   memory */
typedef struct JSVarDef {
    JSAtom var_name;
    /* index into fd->scopes of this variable lexical scope */
    int scope_level;
    /* during compilation:
        - if scope_level = 0: scope in which the variable is defined
        - if scope_level != 0: index into fd->vars of the next
          variable in the same or enclosing lexical scope
       in a bytecode function:
       index into fd->vars of the next
       variable in the same or enclosing lexical scope
    */
    int scope_next;
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1;
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* only used during compilation: function pool index for lexical
       variables with var_kind =
       JS_VAR_FUNCTION_DECL/JS_VAR_NEW_FUNCTION_DECL or scope level of
       the definition of the 'var' variables (they have scope_level =
       0) */
    int func_pool_idx : 24; /* only used during compilation : index in
                               the constant pool for hoisted function
                               definition */
} JSVarDef;

typedef struct JSClosureVar {
    uint8_t is_local : 1;
    uint8_t is_arg : 1;
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* 8 bits available */
    uint16_t var_idx; /* is_local = TRUE: index to a normal variable of the
                    parent function. otherwise: index to a closure
                    variable of the parent function */
    JSAtom var_name;
} JSClosureVar;

//some internal functions used to implement the debugger.
int find_line_num(JSContext *ctx, JSFunctionBytecode *b, uint32_t pc_value);
JSStackFrame* js_debugger_get_current_stackFrame(JSContext *ctx);
JSStackFrame* js_debugger_get_previous_frame(JSStackFrame* sf);
JSValue js_debugger_get_stackframe_current_func(JSStackFrame* sf);
FunctionBytecodeDebuggerInfo* js_debugger_get_bytecode_debugger_info(JSFunctionBytecode* b);
struct FunctionBytecodeDebugInfo* js_debugger_get_bytecode_debug_info(JSFunctionBytecode* b);
JS_BOOL js_debugger_bytecode_has_debug(JSFunctionBytecode* b);
int js_debugger_get_bytecode_cpool_count(JSFunctionBytecode* b);
JSFunctionBytecode* js_debugger_get_children_function(JSFunctionBytecode* b, int cpool_index);
int js_debugger_get_bytecode_len(JSFunctionBytecode* b);
JSFunctionBytecode* js_debugger_get_bytecode(JSValue value);
const uint8_t* js_debugger_get_stackframe_pc(JSStackFrame* sf);
const uint8_t* js_debugger_get_bytecode_buf(JSFunctionBytecode* b);
uint16_t js_debugger_get_bytecode_var_count(JSFunctionBytecode* b);
uint16_t js_debugger_get_bytecode_arg_count(JSFunctionBytecode* bytecode);
JSValue js_debugger_get_stackframe_arg(JSStackFrame* sf, int arg_index);
JSValue js_debugger_get_stackframe_var(JSStackFrame* sf, int var_index);
JS_BOOL js_debugger_is_primitive(JSContext* ctx, JSValue value);
JSVarDef* js_debugger_get_bytecode_vardef(JSFunctionBytecode* bytecode, int vardef_index);
JSVarKindEnum js_debugger_get_vardef_kind(JSVarDef* vardef);
int js_debugger_get_bytecode_closure_var_count(JSFunctionBytecode* bytecode);
JSClosureVar* js_debugger_get_bytecode_closure_var(JSFunctionBytecode* bytecode, int closure_index);
JSValue js_debugger_get_function_closure_value(JSValue function, int closure_index);
JSValue js_debugger_eval(JSContext *ctx, JSValueConst this_obj, JSStackFrame *sf,
                                 const char *input, size_t input_len,
                                 const char *filename, int flags, int scope_idx, int argc, JSValue* argv);
JSValue js_debugger_eval_bytecode_function(JSContext *ctx, JSValueConst this_obj, JSStackFrame *sf, JSValueConst func_obj, int argc, JSValue* argv);
JSValue js_debugger_get_children_function_object(JSContext* ctx, JSFunctionBytecode* b, JSAtom func_name_atom);


JS_BOOL js_debugger_class_has_bytecode(JSClassID class_id);
int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end);
int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end);
int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc, JSObject* val, JSAtom prop);

const char *get_func_name(JSContext *ctx, JSValueConst func);
const uint8_t* js_debugger_get_curr_pc(JSContext *ctx);
int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);

const char* js_debugger_get_type_name(JSContext *ctx, JSValue value);
const char* js_debugger_get_subtype_name(JSContext *ctx, JSValue value);

#endif


// end internal api functions


#ifdef __cplusplus
}
#endif

#endif

#endif
