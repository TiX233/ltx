#ifndef __LTX_CONFIG_H__
#define __LTX_CONFIG_H__

#include "stdint.h"

typedef uint32_t TickType_t;
typedef uint16_t UsType_t;

// 需要空闲任务则打开此宏，并将调度器从主循环转移到最低优先级的软中断中
// #define ltx_cfg_USE_IDLE_TASK
// 需要 tickless 则打开此宏，前提是必须打开空闲任务宏
// #define ltx_cfg_USE_TICKLESS

// 选择一个对应架构的配置文件
#include "ltx_arch_arm_cortex_m.h"
// #include "ltx_arch_xxx.h"


// 未开启空闲任务功能的默认值
#ifndef ltx_cfg_USE_IDLE_TASK
    // 设置调度标志位，表示需要进行调度，可设置为置位软中断标志位
    #define _LTX_SET_SCHEDULE_FLAG()    do{}while(0)
    // 获取调度标志位，可设置为读软中断标志位
    #define _LTX_GET_SCHEDULE_FLAG      1
    // 清除调度标志位，可设置为清除软中断标志位
    #define _LTX_CLEAR_SCHEDULE_FLAG()  do{}while(0)
#endif

#ifndef _LTX_ARCH_SELECTED
    #error "Please select the hardware architecture you are using!"
#endif

// 编译器相关宏定义，偷自 rtthread
#if defined(__ARMCC_VERSION)           /* ARM Compiler */
    #define ltx_section(x)              __attribute__((section(x)))
    #define ltx_used                    __attribute__((used))
    #define ltx_align(n)                __attribute__((aligned(n)))
    #define ltx_weak                    __attribute__((weak))
    #define ltx_inline                  static __inline
    /* module compiling */
    #ifdef LTX_USING_MODULE
        #define LTX_API                 __declspec(dllimport)
    #else
        #define LTX_API                 __declspec(dllexport)
    #endif /* LTX_USING_MODULE */
#elif defined (__IAR_SYSTEMS_ICC__)     /* for IAR Compiler */
    #define ltx_section(x)               @ x
    #define ltx_used                    __root
    #define PRAGMA(x)                   _Pragma(#x)
    #define ltx_align(n)                    PRAGMA(data_alignment=n)
    #define ltx_weak                    __weak
    #define ltx_inline                  static inline
    #define LTX_API
#elif defined (__GNUC__)                /* GNU GCC Compiler */
    #ifndef LTX_USING_LIBC
        /* the version of GNU GCC must be greater than 4.x */
        typedef __builtin_va_list       __gnuc_va_list;
        typedef __gnuc_va_list          va_list;
        #define va_start(v,l)           __builtin_va_start(v,l)
        #define va_end(v)               __builtin_va_end(v)
        #define va_arg(v,l)             __builtin_va_arg(v,l)
    #endif /* LTX_USING_LIBC */
    #define __LTX_STRINGIFY(x...)        #x
    #define LTX_STRINGIFY(x...)          __LTX_STRINGIFY(x)
    #define ltx_section(x)              __attribute__((section(x)))
    #define ltx_used                    __attribute__((used))
    #define ltx_align(n)                __attribute__((aligned(n)))
    #define ltx_weak                    __attribute__((weak))
    #define ltx_noreturn                __attribute__ ((noreturn))
    #define ltx_inline                  static __inline
    #define LTX_API
#elif defined (__ADSPBLACKFIN__)        /* for VisualDSP++ Compiler */
    #define ltx_section(x)              __attribute__((section(x)))
    #define ltx_used                    __attribute__((used))
    #define ltx_align(n)                __attribute__((aligned(n)))
    #define ltx_weak                    __attribute__((weak))
    #define ltx_inline                  static inline
    #define LTX_API
#elif defined (_MSC_VER)
    #define ltx_section(x)
    #define ltx_used
    #define ltx_align(n)                __declspec(align(n))
    #define ltx_weak
    #define ltx_inline                  static __inline
    #define LTX_API
#elif defined (__TI_COMPILER_VERSION__)
    /* The way that TI compiler set section is different from other(at least
        * GCC and MDK) compilers. See ARM Optimizing C/C++ Compiler 5.9.3 for more
        * details. */
    #define ltx_section(x)              __attribute__((section(x)))
    #ifdef __TI_EABI__
        #define ltx_used                __attribute__((retain)) __attribute__((used))
    #else
        #define ltx_used                __attribute__((used))
    #endif
    #define PRAGMA(x)                   _Pragma(#x)
    #define ltx_align(n)                __attribute__((aligned(n)))
    #ifdef __TI_EABI__
        #define ltx_weak                __attribute__((weak))
    #else
        #define ltx_weak
    #endif
    #define ltx_inline                  static inline
    #define LTX_API
#elif defined (__TASKING__)
    #define ltx_section(x)              __attribute__((section(x)))
    #define ltx_used                    __attribute__((used, protect))
    #define PRAGMA(x)                   _Pragma(#x)
    #define ltx_align(n)                __attribute__((__align(n)))
    #define ltx_weak                    __attribute__((weak))
    #define ltx_inline                  static inline
    #define LTX_API
#else
    #error not supported tool chain
#endif /* __ARMCC_VERSION */

#endif // __LTX_CONFIG_H__
