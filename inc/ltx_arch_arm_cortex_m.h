#ifndef __LTX_ARCH_ARM_CORTEX_M_H__
#define __LTX_ARCH_ARM_CORTEX_M_H__

#define _LTX_ARCH_SELECTED

#include "ltx_config.h"

// 开关中断宏
#define _LTX_IRQ_ENABLE()                       __enable_irq()
#define _LTX_IRQ_DISABLE()                      __disable_irq()

// 开启空闲任务的话，设置为触发 最低优先级 的软中断
#ifdef ltx_cfg_USE_IDLE_TASK
    // 设置为置位 PendSV 标志位，触发其运行
    #define _LTX_SET_SCHEDULE_FLAG()            (SCB->ICSR = SCB_ICSR_PENDSVSET_Msk)
    // 设置为读 PendSV 标志位
    #define _LTX_GET_SCHEDULE_FLAG              (SCB->ICSR & SCB_ICSR_PENDSVSET_Msk)
    // 设置为清除 PendSV 标志位
    #define _LTX_CLEAR_SCHEDULE_FLAG()          (SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk)
#endif

// 开启 tickless 的话
#ifdef ltx_cfg_USE_TICKLESS
    // 暂停 systick
    #define _ltx_Sys_systick_pause()            (SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk)
    // 恢复 systick
    #define _ltx_Sys_systick_resume()           (SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk)
    // 设置 systick 重载值
    #define _ltx_Sys_systick_set_reload(reload) (SysTick->LOAD = (uint32_t)(reload))
    // 获取 systick 重载值
    #define _ltx_Sys_systick_get_reload()       (SysTick->LOAD)
    // 清除 systick 计数值为重载值
    #define _ltx_Sys_systick_clr_val()          (SysTick->VAL = 0UL)
    // 获取 systick 计数值
    #define _ltx_Sys_systick_get_val()          (SysTick->VAL)
    // 获取 systick 中断标志位，用于判断是否溢出/重载，但是 arm 的 systick 的中断标志位会在读取后被清除，，，
    #define _ltx_Sys_systick_get_flag()         (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    // 清除 systick 中断标志位，对于 arm cortex-m，COUNTFLAG 为只读位，需通过读取 CTRL 寄存器清除
    #define _ltx_Sys_systick_clr_flag()         ((void)SysTick->CTRL)

    // 告知调度器 systick 的频率
    #define _SYSTICK_FREQ                       8000000
    
    // arm cortex-m systick 最大重载值为 24 位
    #define _SYSTICK_MAX_RELOAD                 0xFFFFFF
    // systick 一个 tick 的计数值(以 1ms 为一个 tick)
    #define _SYSTICK_COUNT_PER_TICK             (_SYSTICK_FREQ/1000)
    // systick 最大 tick 计数值，预留一个，避免溢出
    #define _SYSTICK_MAX_TICK                   (_SYSTICK_MAX_RELOAD/_SYSTICK_COUNT_PER_TICK - 1)
#endif

#endif // __LTX_ARCH_ARM_CORTEX_M_H__
