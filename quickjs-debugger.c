#include "quickjs-debugger.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG
// in thread check request/response of pending commands.
// todo: background thread that reads the socket.
void js_debugger_check(JSContext* ctx, const uint8_t *cur_pc) {
    JSDebuggerInfo *info = js_debugger_info(JS_GetRuntime(ctx));
    if(info->callbacks.cb_debugger_check)
        info->callbacks.cb_debugger_check(ctx, info, cur_pc);
}

void js_debugger_free(JSRuntime *rt, JSDebuggerInfo *info) {
    if(info->package_name)
        free(info->package_name);
    info->package_name = NULL;
    if(!info->debugging_ctx)
        return;
    JS_FreeContext(info->debugging_ctx);
    info->debugging_ctx = NULL;
}

#endif