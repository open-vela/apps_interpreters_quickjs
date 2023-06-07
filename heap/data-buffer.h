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

#ifndef __DATA_BUFFER_H__
#define __DATA_BUFFER_H__
#include <syslog.h>
#include <time.h>

static uint8_t *_g_data_buffer = NULL;
static uint32_t _g_data_buffer_size = 4 * 1024;
static uint32_t _g_data_buffer_offset = 0;

typedef enum _object_type {
  OBJECT_EMPTY = 0,
  OBJECT_RUNTIME,
  OBJECT_CONTEXT,
  OBJECT_SHAPE_HASH,
  OBJECT_ATOM_HASH,
  OBJECT_ATOM_ARRAY,
  OBJECT_JS_STRING,
  OBJECT_OBJECT_LIST,
  OBJECT_JS_PROPERTY,
  OBJECT_JS_MODULE,
  OBJECT_CLASS_PROTO,
  OBJECT_VAR_REF,
  OBJECT_DIRECT_COPY,
  OBJECT_BOUND_FUNCTION,
  OBJECT_MAX,
} object_type;

typedef enum _js_module_type {
  JS_MODULE_EMPTY = 0,
  JS_MODULE_DEF,
  JS_MODULE_REQ,
  JS_MODULE_EXPORT,
  JS_MODULE_STAR_EXPORT,
  JS_MODULE_IMPORT,
} js_module_type;

typedef struct _protocol_data_item {
  uint32_t arm_addr;             // 设备上的地址
  uint32_t object_type : 4;      // shape、object、function
  uint32_t shape_prop_size : 28; // shape:prop_size
  uint32_t shape_hash_size;      // shape:hash_size
  uint32_t data_type : 8;        // runtime、context、object
  uint32_t buffer_size : 24;     //
  uint8_t buffer[0];             //
} protocol_data_item, *p_protocol_data_item;

typedef struct _protocol_data_ {
  uint32_t count; // item 数量
  uint32_t size;  // data整个buffer大小，读写文件可以使用
  protocol_data_item item[0]; //
} protocol_data, *p_protocol_data;

// 分配内存
void *_def_malloc(void *arm_addr, int32_t len) {
  JSMallocState s;
  s.malloc_count = 0;
  s.malloc_size = 0;
  s.malloc_limit = 0xFFFFFFFF;

  void *pc_addr = (void *)js_def_malloc(&s, len);
  if (pc_addr == NULL) {
    return NULL;
  }
  if (arm_addr)
    memcpy(pc_addr, arm_addr, len);
  else
    memset(pc_addr, 0, len);
  return pc_addr;
}

// 内存释放
void _def_free(void *src) {
  JSMallocState s;
  js_def_free(&s, src);
}

void *_def_realloc(void *arm_addr, int32_t len) {
  JSMallocState s;
  s.malloc_count = 0;
  s.malloc_size = 0;
  s.malloc_limit = 0xFFFFFFFF;

  void *pc_addr = (void *)js_def_realloc(&s, arm_addr, len);
  if (pc_addr == NULL) {
    return NULL;
  }
  return pc_addr;
}

// 初始化一个buffer
void _init_data_buffer(uint32_t buffer_size) {
  if (_g_data_buffer) {
    _def_free(_g_data_buffer);
  }

  _g_data_buffer = _def_malloc(NULL, buffer_size);
  memset(_g_data_buffer, 0, buffer_size);
  _g_data_buffer_size = buffer_size;
  _g_data_buffer_offset = sizeof(protocol_data);
  ((p_protocol_data)_g_data_buffer)->count = 0;
}

uint32_t _get_data_buffer_offset(void) { return _g_data_buffer_offset; }

void _flush_data_buffer(void) {
  // 设备上写入文件
#ifndef __NuttX__
  static time_t _timestamp = 0;
  if (!_timestamp) {
    _timestamp = time(NULL);
  }
  char path[100];
  sprintf(path, "./quickjs-%ld.heap", _timestamp);
  FILE *fp = fopen(path, "ab");
  if (fp == NULL) {
    syslog(LOG_ERR, "==========Can't create output file=========== \n");
    return;
  }
  syslog(LOG_ERR, "==========_flush_data_buffer size:%d=count:%d========== \n",
         _g_data_buffer_offset, ((p_protocol_data)_g_data_buffer)->count);
  ((p_protocol_data)_g_data_buffer)->size = _g_data_buffer_offset;
  fwrite(_g_data_buffer, _g_data_buffer_offset, 1, fp);
  fclose(fp);
#endif

  _g_data_buffer_offset = sizeof(protocol_data);
  memset(_g_data_buffer, 0, _g_data_buffer_size);
  ((p_protocol_data)_g_data_buffer)->count = 0;
}

p_protocol_data_item _alloc_protocol_data_item(uint32_t data_size) {
  // 发送数据，大于buffer
  if (data_size + sizeof(protocol_data_item) + sizeof(protocol_data) >
      _g_data_buffer_size) {
    _flush_data_buffer();
    uint32_t new_size = max_uint32(data_size + sizeof(protocol_data_item) +
                                       sizeof(protocol_data),
                                   _g_data_buffer_size * 3 / 2);
    _init_data_buffer(new_size);
  }
  // 发送数据，重新开始计算
  if (data_size + sizeof(protocol_data_item) + _g_data_buffer_offset >
      _g_data_buffer_size) {
    _flush_data_buffer();
  }
  p_protocol_data_item item =
      (p_protocol_data_item)(_g_data_buffer + _g_data_buffer_offset);
  item->buffer_size = data_size;
  // printf("databuffer : %s size:%d offset:%d count:%d \n", __FUNCTION__ ,
  // data_size, _g_data_buffer_offset,
  // ((p_protocol_data)_g_data_buffer)->count);
  _g_data_buffer_offset += data_size + sizeof(protocol_data_item);
  ((p_protocol_data)_g_data_buffer)->count++;
  return item;
}

p_protocol_data_item _alloc_protocol_data_item_header(uint32_t data_size) {
  // 发送数据，大于buffer
  if (data_size + sizeof(protocol_data_item) + sizeof(protocol_data) >
      _g_data_buffer_size) {
    _flush_data_buffer();
    uint32_t new_size = max_uint32(data_size + sizeof(protocol_data_item) +
                                       sizeof(protocol_data),
                                   _g_data_buffer_size * 3 / 2);
    _init_data_buffer(new_size);
  }
  // 发送数据，重新开始计算
  if (data_size + sizeof(protocol_data_item) + _g_data_buffer_offset >
      _g_data_buffer_size) {
    _flush_data_buffer();
  }
  p_protocol_data_item item =
      (p_protocol_data_item)(_g_data_buffer + _g_data_buffer_offset);
  // printf("databuffer : %s size:%d offset:%d count:%d \n", __FUNCTION__ ,
  // data_size, _g_data_buffer_offset,
  // ((p_protocol_data)_g_data_buffer)->count);
  _g_data_buffer_offset += sizeof(protocol_data_item);
  ((p_protocol_data)_g_data_buffer)->count++;
  return item;
}

void *_write_data_buffer(void *data, uint32_t data_size) {
  // 大小不再计算，在申请header时做了2倍预估
  void *buffer = (void *)(_g_data_buffer + _g_data_buffer_offset);
  memcpy(buffer, data, data_size);
  // printf("databuffer : %s size:%d offset:%d count:%d \n", __FUNCTION__ ,
  // data_size, _g_data_buffer_offset,
  // ((p_protocol_data)_g_data_buffer)->count);
  _g_data_buffer_offset += data_size;
  return buffer;
}

#define _write_data_buffer_type_function(type_, data)                          \
  type_ _write_data_buffer_##type_(type_ data) {                               \
    type_ *buffer = (type_ *)(_g_data_buffer + _g_data_buffer_offset);         \
    *buffer = data;                                                            \
    _g_data_buffer_offset += sizeof(type_);                                    \
    return (type_)*buffer;                                                     \
  }

_write_data_buffer_type_function(int, data);
_write_data_buffer_type_function(uint8_t, data);
_write_data_buffer_type_function(uint16_t, data);
_write_data_buffer_type_function(uint32_t, data);
_write_data_buffer_type_function(uint64_t, data);

#define FILL_PROTOCOL_DATA_ITEM(addr, size, type_index)                        \
  p_protocol_data_item ppdi = _alloc_protocol_data_item(size);                 \
  ppdi->arm_addr = (uint32_t)addr;                                             \
  ppdi->shape_hash_size = 0;                                                   \
  ppdi->shape_prop_size = 0;                                                   \
  ppdi->data_type = type_index;                                                \
  memcpy(ppdi->buffer, (void *)ppdi->arm_addr, ppdi->buffer_size);

#define FILL_PROTOCOL_DATA_ITEM_ALIGNMENT(addr, size, type_index, function)    \
  p_protocol_data_item ppdi = _alloc_protocol_data_item_header(size * 2);      \
  ppdi->arm_addr = (uint32_t)addr;                                             \
  ppdi->shape_hash_size = 0;                                                   \
  ppdi->shape_prop_size = 0;                                                   \
  ppdi->data_type = type_index;                                                \
  ppdi->buffer_size = _get_data_buffer_offset();                               \
  function(addr);                                                              \
  ppdi->buffer_size = _get_data_buffer_offset() - ppdi->buffer_size;

#endif
