#ifndef __LTX_CONFIG_H__
#define __LTX_CONFIG_H__

#include "stdint.h"

/* ------------------- 系统时钟单位 tick 数据类型 ------------------- */
typedef uint32_t TickType_t;

/* ------------------------- 核心/线程 数量 ------------------------- */
#define ltx_cfg_CORE_NUM            2

/* ------------------- 空闲钩子与 tickless 开关宏 ------------------- */
// 需要空闲钩子则打开此宏，不打开则事件循环将不断尝试弹出事件队列头
// #define ltx_cfg_USE_IDLE_HOOK
// 需要 tickless 钩子则打开此宏，V4 版本将不会由调度器操作硬件定时器，而是通过 tickless 钩子传递下次唤醒的时间，由外部决定
// #define ltx_cfg_USE_TICKLESS

// 选择一种时间驱动方案
// 1、将 ltx_Sys_tick_tack() 放置到硬件定时器中断内弹出闹钟
#define SYSTICK_TYPE_INTERRUPT      1
// 2、调度器通过空闲时判断外部时间戳来决定是否弹出闹钟
#define SYSTICK_TYPE_TIMESTAMP      2

#define ltx_cfg_SYSTICK_TYPE        SYSTICK_TYPE_INTERRUPT

/* ------------------- 选择一个对应架构的配置文件 ------------------- */
#include "ltx_arch_arm_cortex_m.h"
// #include "ltx_arch_xxx.h"

/* ------------------- 以下内容用户一般不需要修改 ------------------- */

// 未开启空闲钩子功能的默认值
#ifndef ltx_cfg_USE_IDLE_HOOK
    // 设置调度标志位，表示需要进行调度，可配置为 发布 rtos 信号量、产生 cpu 唤醒事件 等等
    #define _LTX_SET_SCHEDULE_FLAG()    do{}while(0)
    // 获取调度标志位
    // #define _LTX_GET_SCHEDULE_FLAG      1
    // 清除调度标志位
    // #define _LTX_CLEAR_SCHEDULE_FLAG()  do{}while(0)
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
