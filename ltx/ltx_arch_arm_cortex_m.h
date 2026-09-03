#ifndef __LTX_ARCH_ARM_CORTEX_M_H__
#define __LTX_ARCH_ARM_CORTEX_M_H__

#include "ltx_config.h"

// 有时候，内存不同步可能会导致一些系统配置不起作用，从而出现一些奇奇怪怪的现象
// 您可以考虑合理插入 DMB/DSB/ISB 等等指令来冲刷流水线

// 进出临界区宏
#if (ltx_cfg_CORE_NUM == 1)
    // 单核只用关中断
    #define _LTX_CRITICAL_INTO()                do{__disable_irq(); __DMB();}while(0)
    #define _LTX_CRITICAL_OUTO()                do{__DMB(); __enable_irq();}while(0)
#else
    typedef uint32_t spin_num_t;
    extern volatile spin_num_t __g_spin_lock;
    // 多核则先关中断再自旋
    #define _LTX_CRITICAL_INTO()                do{ \
                                                    __disable_irq(); /* 防止核心自己的任务与中断死锁 */ \
                                                    while(1){ /* 自旋，防止多核竞争 */ \
                                                        if(__LDREXW(&__g_spin_lock) == 0){ /* 读取并标记独占 */\
                                                            if(__STREXW(1, &__g_spin_lock) == 0){ /* 尝试写入 1 */ \
                                                                break; /* 获取锁成功 */ \
                                                            } \
                                                        } \
                                                    } \
                                                    __DMB(); \
                                                }while(0)

    #define _LTX_CRITICAL_OUTO()                do{ \
                                                    __DMB(); \
                                                    __STREXW(0, &__g_spin_lock); \
                                                    __enable_irq(); \
                                                }while(0)
#endif

// 开启空闲钩子的话，设置触发唤醒 cpu 事件，如果事件循环作为 rtos 的一个线程，那么可以设置为发送信号量
#ifdef ltx_cfg_USE_IDLE_HOOK
    #define _LTX_SET_SCHEDULE_FLAG()            __SEV()
    // 清除标志位
    // #define _LTX_CLEAR_SCHEDULE_FLAG()          do{__SEVL(); __WFE();}while(0)
#endif

#endif // __LTX_ARCH_ARM_CORTEX_M_H__
