cmake_minimum_required(VERSION 3.16)
project(jsc)

set(CONFIG_LTO y)
set(prefix "/usr/local")

set(QUICKJS_DIR ${CMAKE_CURRENT_LIST_DIR})

if(CONFIG_DARWIN)
  set(CONFIG_CLANG y)
  set(CONFIG_DEFAULT_AR y)
endif()

if(CONFIG_WIN32)
  if(CONFIG_M32)
    set(CROSS_PREFIX i686-w64-mingw32-)
  else()
    set(CROSS_PREFIX x86_64-w64-mingw32-)
  endif()
else()
  set(CROSS_PREFIX)
endif()

set(QUICKJS_COMMON_OPT)
set(QJSC_OPT)
if(CONFIG_CLANG)
  set(HOST_CC clang)
  set(CC ${CROSS_PREFIX}clang)
  list(APPEND QUICKJS_COMMON_OPT -g -Wall)
  list(APPEND QUICKJS_COMMON_OPT -Wextra)
  list(APPEND QUICKJS_COMMON_OPT -Wno-sign-compare)
  list(APPEND QUICKJS_COMMON_OPT -Wno-missing-field-initializers)
  list(APPEND QUICKJS_COMMON_OPT -Wundef -Wuninitialized)
  list(APPEND QUICKJS_COMMON_OPT -Wunused -Wno-unused-parameter)
  list(APPEND QUICKJS_COMMON_OPT -Wwrite-strings)
  list(APPEND QUICKJS_COMMON_OPT -Wchar-subscripts -funsigned-char)
  if(CONFIG_DEFAULT_AR)
    set(AR ${CROSS_PREFIX}ar)
  else()
    if(CONFIG_LTO)
      set(AR ${CROSS_PREFIX}llvm-ar)
    else()
      set(AR ${CROSS_PREFIX}ar)
    endif()
  endif()
else()
  set(HOST_CC gcc)
  set(CC ${CROSS_PREFIX}gcc)
  list(APPEND QUICKJS_COMMON_OPT -g -Wall)
  list(APPEND QUICKJS_COMMON_OPT -Wno-array-bounds -Wno-format-truncation)
  if(CONFIG_LTO)
    set(CMAKE_AR ${CROSS_PREFIX}gcc-ar)
    set(CMAKE_RANLIB ${CROSS_PREFIX}gcc-ranlib)
  else()
    set(AR ${CROSS_PREFIX}ar)
  endif()
endif()

set(STRIP ${CROSS_PREFIX}strip)

if(CONFIG_WERROR)
  list(APPEND QUICKJS_COMMON_OPT -Werror)
endif()

set(QUICKJS_COMMON_DEF)
set(QUICKJS_DEF)
set(QJSC_DEF)
list(APPEND QUICKJS_COMMON_DEF -D_GNU_SOURCE
     -DCONFIG_VERSION=${QUICKJS_VERSION})

if(CONFIG_BIGNUM STREQUAL y)
  list(APPEND QUICKJS_COMMON_DEF -DCONFIG_BIGNUM)
endif()

list(APPEND QJSC_OPT -O2)

if(CONFIG_LTO)
  list(APPEND CFLAGS_SMALL -flto)
  list(APPEND QUICKJS_COMMON_OPT -flto)
  list(APPEND LDFLAGS -flto)
endif()

if(CONFIG_PROFILE)
  list(APPEND CFLAGS -p)
  list(APPEND LDFLAGS -p)
endif()

if(CONFIG_ASAN)
  list(APPEND CFLAGS -fsanitize=address -fno-omit-frame-pointer)
  list(APPEND LDFLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()

if(CROSS_PREFIX)
  set(QJSC_CC gcc)
  set(QJSC ./host-qjsc)
else()
  set(QJSC_CC ${CC})
  set(QJSC ./qjsc)
endif()

list(APPEND QUICKJS_COMMON_OPT -O2)

list(APPEND QJSC_OPT ${QUICKJS_COMMON_OPT})

list(APPEND QUICKJS_DEF ${QUICKJS_COMMON_DEF})

list(APPEND QJSC_DEF ${QUICKJS_COMMON_DEF})
list(APPEND QJSC_DEF -DCONFIG_LTO -DCONFIG_CC=\"gcc\"
     -DCONFIG_PREFIX=\"/usr/local\")

set(QUICKJS_SRC
    ${QUICKJS_DIR}/quickjs.c
    ${QUICKJS_DIR}/libbf.c
    ${QUICKJS_DIR}/libunicode.c
    ${QUICKJS_DIR}/libregexp.c
    ${QUICKJS_DIR}/cutils.c
    ${QUICKJS_DIR}/quickjs-libc.c
    ${QUICKJS_DIR}/profile_utils.c
    ${QUICKJS_DIR}/quickjs-debugger.c
    ${QUICKJS_DIR}/quickjs-memorydump.c)
add_library(quickjs ${QUICKJS_SRC})
target_compile_options(quickjs PRIVATE ${QUICKJS_COMMON_OPT})
target_compile_definitions(quickjs PRIVATE ${QUICKJS_DEF})

include(qjsc.cmake)
