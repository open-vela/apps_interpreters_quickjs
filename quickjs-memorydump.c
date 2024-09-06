
#include "quickjs-memorydump.h"
#include "quickjs-debugger.h"

#ifdef CONFIG_INTERPRETERS_QUICKJS_DEBUG

void CDP_start_memory_tracking(JSRuntime *rt) {
  getDumpMemoryInfo(rt)->is_started_memory_tracking = 1;
}

void CDP_stop_memory_tracking(JSRuntime *rt) {
  getDumpMemoryInfo(rt)->is_started_memory_tracking = 0;
}

void CDP_create_obj_name(CDP_memory_str_val* name, const char *str) {
  CDPFreeString flag = CDP_FREE_NO;
  name->flag = flag;
  name->name = str;
}

CDP_memory_str_val CDP_get_obj_name(JSRuntime *rt, JSAtom atom) {
  CDPFreeString flag = CDP_FREE_YES;
  CDP_memory_str_val name = {
    .flag = flag,
    .name = NULL,
  };

  if (atom) {
    name.name = JS_AtomToCString(js_debugger_info(rt)->currCtx, atom);
  } else {
    name.name = CDP_UNKNOW_DEFAULT_NAME;
    name.flag = CDP_FREE_NO;
  }
  return name;
}

static int64_t memoryId = 200;
int64_t getDumpMemoryId(void){
  return memoryId++;
};

void CDP_get_stats_update_info(JSRuntime *rt, JSGCObjectHeader *h) {
  int64_t count = 1;
  int64_t size = 1;
  memory_object_id id = (memory_object_id)(uintptr_t)h;
  CDP_get_gc_obj_count_and_size(rt, h, &count, &size);
  getDumpMemoryInfo(rt)->GC_obj_change(rt, id, 1, size);
}

void CDP_remove_gc_obj(JSRuntime *rt, JSGCObjectHeader *h) {
  memory_object_id id = (memory_object_id)(uintptr_t)h;
  getDumpMemoryInfo(rt)->GC_obj_change(rt, id, 0, 0);
}

Macro_heap_info CDP_get_heap_usage(JSRuntime *rt) {
  JSMemoryUsage s;
  Macro_heap_info macro_heap_info;
  JS_ComputeMemoryUsage(rt, &s);
  macro_heap_info.totalSize = s.malloc_size;
  macro_heap_info.usedSize = s.memory_used_size;
  macro_heap_info.totalCount = s.malloc_count;
  return macro_heap_info;
}

char *CDP_int_to_string(int num, char *str, int radix) {
  char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 索引表
  unsigned unum; // 存放要转换的整数的绝对值,转换的整数可能是负数
  int i = 0, j,
      k; // i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

  // 获取要转换的整数的绝对值
  if (radix == 10 && num < 0) // 要转换成十进制数并且是负数
  {
    unum = (unsigned)-num; // 将num的绝对值赋给unum
    str[i++] = '-'; // 在字符串最前面设置为'-'号，并且索引加1
  } else
    unum = (unsigned)num; // 若是num为正，直接赋值给unum

  // 转换部分，注意转换后是逆序的
  do {
    str[i++] =
        index[unum %
              (unsigned)
                  radix]; // 取unum的最后一位，并设置为str对应位，指示索引加1
    unum /= radix; // unum去掉最后一位

  } while (unum); // 直至unum为0退出循环

  str[i] = '\0'; // 在字符串最后添加'\0'字符，c语言字符串以'\0'结束。

  // 将顺序调整过来
  if (str[0] == '-')
    k = 1; // 如果是负数，符号不用调整，从符号后面开始调整
  else
    k = 0; // 不是负数，全部都要调整

  char temp; // 临时变量，交换两个值时用到
  for (j = k; j <= (i - 1) / 2;
       j++) // 头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
  {
    temp = str[j];               // 头部赋值给临时变量
    str[j] = str[i - 1 + k - j]; // 尾部赋值给头部
    str[i - 1 + k - j] = temp; // 将临时变量的值(其实就是之前的头部值)赋给尾部
  }

  return str; // 返回转换后的字符串
}
#endif