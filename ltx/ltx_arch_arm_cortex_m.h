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
    typedef volatile uint32_t spin_type_t;
    extern spin_type_t __g_spin_lock;
    // 多核则先关中断再自旋
    // 在单片机上，可以关中断保证核心/线程不被挂起，所以自旋锁会比 cas 高效，因为只有释放锁导致其它核心 cache miss 才会让其它核心产生总线流量
    // 而 pc 上，如果持有自旋锁后线程挂起，那么其它所有等待自旋锁的任务都得停住等它恢复，所以 cas 虽然会频繁访问总线刷新数据，但是不会一损俱损
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

// 空闲休眠相关配置
#ifdef ltx_cfg_USE_IDLE_SLEEP
    // 恢复调度器执行，默认为唤醒 cpu
    // 如果调度器跑在 rtos 的一个线程，那么可以设置为发送信号量
    #define _LTX_SET_SCHEDULE_FLAG()            __SEV()

    // 调度器空闲休眠回调，默认为 cpu 休眠，用户也可以当成空闲钩子使用，比如可以用来统计 cpu 使用率或者是否进入深度休眠等等
    // 如果调度器跑在 rtos 的一个线程，那么可以设置为等待信号量，超时时间为 最近一个 alalrm 的触发时间
    #define ltx_Sys_idle_in(core_id)            do{__WFE();}while(0)
#endif

// tickless 相关配置
#ifdef ltx_cfg_USE_TICKLESS
    // 设置下次唤醒的时间戳
    // 调度器会事件队列空闲后调用 ltx_Sys_set_next_wake 告知外部，
    // 由用户根据不同平台实现，总之用户平台需要在 tick_stamp 时刻调用一次 _LTX_SET_SCHEDULE_FLAG(); 唤醒调度器
    // 实际休眠时间可以比 ticks 小，因为醒来后调度器还会判断一次时间戳，然后继续传递新值要求休眠新 ticks
    // 如果实际休眠时间比 ticks 大，调度器也能正确处理需要弹出的 alarm，但是会影响任务实时性
    #if 1
        // 样例1：（控制硬件定时器中断下次触发的计数值）
        // （可自定义进 tickless 的阈值，比如离下次唤醒小于 5 tick（或者 RTC 粒度可能不够），就直接 _LTX_SET_SCHEDULE_FLAG(); 让 cpu 干脆别睡了）
        // 设置硬件定时器（或者 RTC）下次触发的倒计时，定时器中断服务函数内调用 _LTX_SET_SCHEDULE_FLAG();
        // 这里的 _LTX_SET_SCHEDULE_FLAG(); 可以是 __SEV(); 发送 cpu 唤醒事件
        // 然后 ltx_Sys_idle_in 里面写 __WFE(); 等待唤醒事件
        #define ltx_Sys_set_next_wake(tick_stamp)       do{ \
                                                            if(tick_stamp - ltx_Sys_get_tick() < 5){_LTX_SET_SCHEDULE_FLAG();} \
                                                            else { \
                                                                _LTX_SET_SCHEDULE_FLAG(); /* 要用 tickless 记得删掉这个 set flag */ \
                                                                /* 这里写设置 rtc 下次闹钟的时间或者配置定时器计数值 */ \
                                                                /* 记得在相关中断里面写 _LTX_SET_SCHEDULE_FLAG(); */ \
                                                            } \
                                                        }while(0)
    #elif 0
        // 样例2：（调度器跑在 RTOS 一个线程内）
        // 这里什么都不用干，在 ltx_Sys_idle_in 里面写等待信号量，
        #define ltx_Sys_set_next_wake(tick_stamp)       do{/* 啥也不用干，rtos 免费午餐 */}while(0)
    #endif

#endif

#endif // __LTX_ARCH_ARM_CORTEX_M_H__
