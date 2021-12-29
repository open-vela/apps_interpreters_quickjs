#include "quickjs-debugger.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG
void js_debugger_exception(JSContext *ctx) {
    JSDebuggerInfo *info = js_debugger_info(JS_GetRuntime(ctx));
    if (info->exception_stop_state == JS_DEBUGGER_EXCEPITON_STOP_NONE)
        return;
    if (info->is_debugging)
        return;
    info->ctx = ctx;
    if(info->callbacks.cb_debugger_paused)
        info->callbacks.cb_debugger_paused(info, PAUSE_REASON_other, js_debugger_get_curr_pc(ctx));
    //handle paused situation
    info->is_paused = 1;
    js_debugger_check(ctx, NULL);
}

// in thread check request/response of pending commands.
// todo: background thread that reads the socket.
void js_debugger_check(JSContext* ctx, const uint8_t *cur_pc) {
    JSDebuggerInfo *info = js_debugger_info(JS_GetRuntime(ctx));
    if(info->callbacks.cb_debugger_check)
        info->callbacks.cb_debugger_check(ctx, info, cur_pc);
}

void js_debugger_free(JSRuntime *rt, JSDebuggerInfo *info) {
    if(!info->debugging_ctx)
        return;
    JS_FreeContext(info->debugging_ctx);
    info->debugging_ctx = NULL;
}

#endif