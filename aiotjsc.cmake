cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
project(aiotjsc)

set(QUICKAPP_DIR ${APPDIR}/frameworks/quickapp)
set(AIOTQJSC_SRCS)
list(APPEND AIOTQJSC_SRCS ${QUICKAPP_DIR}/src/ajs_compile.cpp)
list(APPEND AIOTQJSC_SRCS ${QUICKAPP_DIR}/src/ajs_log.cpp)
list(APPEND AIOTQJSC_SRCS ${QUICKAPP_DIR}/src/jse/quickjs/jse_quickjs.cpp)

set(QUICKJS_COMMON_OPT)
set(AIOTJSC_OPT)

set(QUICKJS_COMMON_DEF)
set(QUICKJS_DEF)
set(AIOTJSC_DEF)
list(APPEND QUICKJS_COMMON_DEF -D_GNU_SOURCE
     -DCONFIG_VERSION=${QUICKJS_VERSION})

if(CONFIG_BIGNUM STREQUAL y)
  list(APPEND QUICKJS_COMMON_DEF -DCONFIG_BIGNUM)
endif()

list(APPEND AIOTJSC_OPT -O2)

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

list(APPEND QUICKJS_COMMON_OPT -O2)

list(APPEND AIOTJSC_OPT ${QUICKJS_COMMON_OPT})

list(APPEND QUICKJS_DEF ${QUICKJS_COMMON_DEF})

list(APPEND AIOTJSC_DEF ${QUICKJS_COMMON_DEF})
list(APPEND AIOTJSC_DEF -DCONFIG_LTO -DCONFIG_CC=\"gcc\"
     -DCONFIG_PREFIX=\"/usr/local\")

target_compile_options(quickjs PRIVATE ${QUICKJS_COMMON_OPT})
target_compile_definitions(quickjs PRIVATE ${QUICKJS_DEF})

add_executable(aiotjsc ${AIOTQJSC_SRCS})
target_include_directories(aiotjsc PUBLIC
${APPDIR}/interpreters/quickjs/
${APPDIR}/system/libuv/libuv/include/
${QUICKAPP_DIR}/src/
${QUICKAPP_DIR}/src/jse/
${QUICKAPP_DIR}/src/jse/quickjs/
${QUICKAPP_DIR}/src/framework/
${QUICKAPP_DIR}/src/gui/
)
target_compile_options(aiotjsc PRIVATE ${AIOTJSC_OPT})
target_compile_definitions(aiotjsc PRIVATE ${AIOTJSC_DEF})

target_link_libraries(aiotjsc quickjs)

