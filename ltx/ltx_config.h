#ifndef __LTX_CONFIG_H__
#define __LTX_CONFIG_H__

#include "stdint.h"

/* ------------------- 系统时钟单位 tick 数据类型 ------------------- */
// V4 改采用时间戳调度，不再由调度器管理时间，总之性能更高解耦更好
// 用户根据自身平台提供 ltx_Sys_get_tick 即可，灵活性更高（可以用更高精度的时间单位作为 1tick 而不用产生那么高频率的中断）并且利于 tickless
// 一般提供 uint32_t 的毫秒时间戳即可，不必担心溢出，除非您打算延时超过 0xFFFFFFFE 毫秒（约 50 天）
// 在 32 位机器上，64 位数据的计算耗时是 32 位的 5 倍以上，所以这里特地规避了一般时间戳调度需要提供 64 位时间戳的情况，并且不用担心溢出
// 总之根据业务中最大的延时时间确定类型定即可，假如业务里最大的延时不超过 255 毫秒，那么您甚至可以使用 uint8_t 作为 TickType_t
// 这并不代表 ltx V4 是直接将延时时间存储在闹钟内，那样的话 tickless 醒来后要遍历所有闹钟删减他们的延时，时间复杂度会来到 O(n)
// V4 闹钟节点只会存储距离前个闹钟节点的触发时间间隔，所以 tickless 醒来后只需要弹出首(几)个闹钟即可，时间复杂度是优雅的 O(1)
// 为什么闹钟节点不存储被触发的时间戳？因为如果时间戳溢出会导致闹钟链表顺序错乱
typedef uint32_t TickType_t;
// 最大延时
#define LTX_MAX_TICK                    (0xFFFFFFFF-1)
// 无限延时，如果设置闹钟时给的是这个那么就不会将闹钟加入闹钟链表
#define LTX_INFINITE_TICK               (0xFFFFFFFF)

/* ------------------------- 核心/线程 数量 ------------------------- */
#define ltx_cfg_CORE_NUM                1

/* ------------------- 空闲休眠与 tickless 开关宏 ------------------- */
// 需要空闲休眠则打开此宏，不打开则事件循环将不断尝试弹出事件队列头，打开后会进入用户实现的休眠回调
// V4 版本将不会由调度器操作硬件定时器，而是通过 ltx_hook_idle_in 传递下次唤醒的时间，由外部决定唤醒信号发送时机
#define ltx_cfg_USE_IDLE_SLEEP

/* ------------------- 选择一个对应架构的配置文件 ------------------- */
#include "ltx_arch_arm_cortex_m.h"
// #include "ltx_arch_xxx.h"

/* ------------------- 以下内容用户一般不需要修改 ------------------- */

// 多核下 ctx 防重入钩子
#if 1
#if (ltx_cfg_CORE_NUM > 1)
    #define ltx_cfg_USE_CTX
    // 调用订阅者回调前，离开临界区前的自定义钩子
    // 此处为做标记防止 ctx 任务的等待事件回调和超时回调同时被多核触发导致业务重入
    // 也就是将本该在单核下的业务函数内处理的内容在多核下搬到 ltx 临界区内
    #define ltx_hook_before_user_call_back()    do { \
                                                    if(pSubscriber->callback_func == _co_alarm_cb){ \
                                                        struct coro_stu *pCo = container_of(pSubscriber, struct coro_stu, subscriber_alarm); \
                                                        if(pCo->topic_wait_for != NULL){ /* 等待事件超时 */ \
                                                            /* 取消订阅事件，也许可以不做检查，暂时先保留 if */ \
                                                            if(pCo->subscriber_topic.prev != NULL){ \
                                                                pCo->subscriber_topic.prev->next = pCo->subscriber_topic.next; \
                                                                if(pCo->subscriber_topic.next != NULL){ \
                                                                    pCo->subscriber_topic.next->prev = pCo->subscriber_topic.prev; \
                                                                    pCo->subscriber_topic.next = NULL; \
                                                                } \
                                                                pCo->subscriber_topic.prev = NULL; \
                                                            } \
                                                            pCo->topic_wait_for = NULL; \
                                                            pCo->something |= 0x80000000; \
                                                        } \
                                                    }else if(pSubscriber->callback_func == _co_subscriber_cb){ \
                                                        struct coro_stu *pCo = container_of(pSubscriber, struct coro_stu, subscriber_topic); \
                                                        /* 关闭超时闹钟 */ \
                                                        __ltx_MC_Alarm_remove(&pCo->alarm_next_run); \
                                                        /* 取消订阅该事件 */ \
                                                        if(pSubscriber->prev != NULL){ \
                                                            pSubscriber->prev->next = pSubscriber->next; \
                                                            if(pSubscriber->next != NULL){ \
                                                                pSubscriber->next->prev = pSubscriber->prev; \
                                                                pSubscriber->next = NULL; \
                                                            } \
                                                            pSubscriber->prev = NULL; \
                                                        } \
                                                        pCo->topic_wait_for = NULL; \
                                                        callback_retval |= 0x02; \
                                                    } \
                                                }while(0)
    #define ltx_hook_after_user_call_back()     do{ \
                                                    if(callback_retval&0x02){ \
                                                        callback_retval = 0; \
                                                    } \
                                                }while(0)
#else
    #define ltx_hook_before_user_call_back()
    #define ltx_hook_after_user_call_back()
#endif
#endif

// 未开启空闲休眠功能的默认值
#ifndef ltx_cfg_USE_IDLE_SLEEP
    // 设置调度标志位，表示需要进行调度，可配置为 发布 rtos 信号量、产生 cpu 唤醒事件 等等
    #define _LTX_SET_SCHEDULE_FLAG()    do{}while(0)
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
