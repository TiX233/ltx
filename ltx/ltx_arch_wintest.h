#ifndef __LTX_ARCH_WIN_TEST_H__
#define __LTX_ARCH_WIN_TEST_H__

#include "ltx_config.h"

// 有时候，内存不同步可能会导致一些系统配置不起作用，从而出现一些奇奇怪怪的现象
// 您可以考虑合理插入 DMB/DSB/ISB 等等指令来冲刷流水线

// 进出临界区宏
typedef CRITICAL_SECTION spin_type_t;
extern spin_type_t __g_spin_lock;
#define _LTX_CRITICAL_INTO()                EnterCriticalSection(&__g_spin_lock)
#define _LTX_CRITICAL_OUTO()                LeaveCriticalSection(&__g_spin_lock)

#define ltx_cfg_USE_SPIN_LOCK

// 系统时间戳获取，需要注意，win 下毫秒时间戳会有十几毫秒误差，所以有些时候闹钟的响应不会那么准，这不是 ltx 的问题
#define ltx_Sys_get_tick()                  (TickType_t)GetTickCount()

// 空闲休眠相关配置
#ifdef ltx_cfg_USE_IDLE_SLEEP
    extern HANDLE g_hSemaphore;
    // 恢复调度器执行，默认为唤醒 cpu
    // 如果调度器跑在 rtos 的一个线程，那么可以设置为发送信号量
    #define _LTX_SET_SCHEDULE_FLAG()        ReleaseSemaphore(g_hSemaphore, 1, NULL)

    // 实际休眠时间可以比 ticks 小，因为醒来后调度器还会判断一次时间戳，然后继续传递新值要求休眠新 ticks
    // 如果实际休眠时间比 ticks 大，调度器也能正确处理需要弹出的 alarm，但是会影响任务实时性
    // 如果调度器是跑在 rtos 的一个线程内，那么可以改成等待信号量，超时时间就用 sleep_ticks，并将 _LTX_SET_SCHEDULE_FLAG(); 设置为发送信号量
    #define ltx_hook_idle_in(core_id, sleep_ticks)  do{ \
                                                        printf("---(%d)idle in, slp: %d---(%d)\n", core_id, sleep_ticks, ltx_Sys_get_tick()); \
                                                        /* 等待信号量 */ \
                                                        DWORD dwWaitResult = WaitForSingleObject(g_hSemaphore, sleep_ticks); \
                                                        printf("---(%d)idle out, ret: %ld---(%d)\n", core_id, dwWaitResult, ltx_Sys_get_tick()); \
                                                    }while(0)
#endif

#endif // __LTX_ARCH_WIN_TEST_H__
