#ifndef __LTX_CONFIG_H__
#define __LTX_CONFIG_H__

// 开关中断宏
#define _LTX_IRQ_ENABLE()           __enable_irq()
#define _LTX_IRQ_DISABLE()          __disable_irq()

// 设置调度标志位，表示需要进行调度，可设置为置位 PendSV 标志位
#define _LTX_SET_SCHEDULE_FLAG()    do{}while(0)
// 获取调度标志位，可设置为读 PendSV 标志位
#define _LTX_GET_SCHEDULE_FLAG      1
// 清除调度标志位，可设置为清除 PendSV 标志位
#define _LTX_CLEAR_SCHEDULE_FLAG()  do{}while(0)

#endif // __LTX_CONFIG_H__
