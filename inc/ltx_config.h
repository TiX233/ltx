#ifndef __LTX_CONFIG_H__
#define __LTX_CONFIG_H__

// 需要空闲任务则打开此宏，并将调度器从主循环转移到最低优先级的软中断中，如 pendsv
// #define USE_IDLE_TASK

// 开关中断宏
#define _LTX_IRQ_ENABLE()           __enable_irq()
#define _LTX_IRQ_DISABLE()          __disable_irq()

#ifdef USE_IDLE_TASK // 开启空闲任务功能
    // 设置为置位 PendSV 标志位，触发其运行
    #define _LTX_SET_SCHEDULE_FLAG()    do{SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;}while(0)
    // 设置为读 PendSV 标志位
    #define _LTX_GET_SCHEDULE_FLAG      (SCB->ICSR & SCB_ICSR_PENDSVSET_Msk)
    // 设置为清除 PendSV 标志位
    #define _LTX_CLEAR_SCHEDULE_FLAG()  do{SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;}while(0)
#else // 不开启空闲休眠功能
    // 设置调度标志位，表示需要进行调度，可设置为置位 PendSV 标志位
    #define _LTX_SET_SCHEDULE_FLAG()    do{}while(0)
    // 获取调度标志位，可设置为读 PendSV 标志位
    #define _LTX_GET_SCHEDULE_FLAG      1
    // 清除调度标志位，可设置为清除 PendSV 标志位
    #define _LTX_CLEAR_SCHEDULE_FLAG()  do{}while(0)
#endif

#endif // __LTX_CONFIG_H__
