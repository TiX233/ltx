/**
 * @file ltx.h
 * @author realTiX
 * @brief 简易的裸机调度框架，主要由定时器、闹钟和发布订阅机制构成。调度器可运行在软中断中，实现空闲休眠能力
 * @version 3.0
 * @date 2025-08-15 (0.1)
 *       2025-08-18 (0.2, 修复在 remove 或 unsubscribe 时没有成员的话会访问到空指针的 bug)
 *       2025-09-02 (0.3, 修复 alarm 会多延时一个 tick 的 bug，移除记录闹钟超时时间的功能)
 *       2025-10-05 (0.4, 添加获取微秒数的 api)
 *       2025-10-10 (0.5, 修复无法移除 timer 的 bug)
 *       2025-10-11 (0.6, 修复获取微秒数错为其补数的 bug)
 *       2025-11-17 (0.7, 优化组件添加内部实现，更正 timer 的 tick_reload 的注释描述)
 *       2025-11-19 (0.8, 修复组件添加忘记检查是否已经存在的 bug，补充添加订阅的内部实现优化)
 *       2025-11-24 (0.9, 将命名从 rtx 改为 ltx，避免重名)
 *       2025-12-01 (0.10, 将话题与闹钟 flag 触发由 ++ 操作改为 置 1 操作)
 *       2025-12-07 (0.11, 将话题调度改为先置 flag 为 0 再调用订阅者回调，提高实时性与可拓展性)
 *       2025-12-12 (0.12, 优化闹钟调度中的移除操作，优化定时器发布话题操作)
 *       2026-01-02 (0.13, 删除添加组件时无意义且有风险的置新元素 next 为 null)
 *       2026-01-04 (0.14, 恢复添加组件时置新元素 next 为 null，不然不能避免重复添加导致的连成环（失忆这一块）)
 *       2026-01-05 (0.15, 添加组件结构体默认参数配置宏；修复潜在问题：在订阅者回调中取消订阅可能导致订阅者链表后续所有订阅者丢失此次对话题的响应)
 *       2026-01-06 (0.16, 增加活跃组件链表尾指针，将添加组件效率由 O(n) 优化为 O(1))
 * 
 *       2026-01-12 (2.0, 重构，支持空闲任务；
 *                              不再遍历组件活跃链表而是弹出事件队列头，效率由 O(n) 优化为 O(1)，且为 tickless 做准备；
 *                              增删组件效率也由 O(n) 优化为 O(1)；
 *                              组件增删升级原子操作，可在中断使用)
 *       2026-01-23 (2.1, 创建闹钟时补充标志位清除，避免已经触发且移除但还没执行的闹钟重新被添加后不推迟执行的极端情况)
 * 
 *       2026-01-27 (3.0, 重构，支持 tickless，支持空闲钩子；
 *                              删除软件定时器，全由闹钟实现；
 *                              将闹钟改为顺序插入，每个闹钟存储与前一个的时间差，systick 弹出效率从 O(N) 变为 O(1)，且便于 tickless)
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef __LTX_H__
#define __LTX_H__

// #include "main.h"
#include "ltx_config.h"

// 从结构体成员计算结构体指针
// 好像不能直接用 linux 里的，从 rtthread 里偷一个
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

// 组件结构体初始化默认参数
#define _LTX_TOPIC_DEAFULT_CONFIG(self)                 {.flag_is_pending = 0, .subscriber_head = {.prev = NULL, .next = NULL}, .subscriber_tail = &(self.subscriber_head), .next = NULL}
#define _LTX_SUBSCRIBER_DEAFULT_CONFIG(callback)        {.callback_func = callback, .prev = NULL, .next = NULL}
// #define _LTX_ALARM_DEAFULT_CONFIG                       {.topic = _LTX_TOPIC_DEAFULT_CONFIG, .prev = NULL, .next = NULL}

// 话题订阅者
struct ltx_Topic_subscriber_stu {
    void (*callback_func)(void *param);

    struct ltx_Topic_subscriber_stu *prev;
    struct ltx_Topic_subscriber_stu *next;
};

// 话题
struct ltx_Topic_stu {
    uint8_t flag_is_pending; // 话题就绪标志位，只有使用 ltx_Topic_publish 才会将其置一，手动置一无效。表示在就绪队列但是还没执行，此时手动置零可以取消这次执行

    struct ltx_Topic_subscriber_stu subscriber_head;
    struct ltx_Topic_subscriber_stu *subscriber_tail;

    struct ltx_Topic_stu *next;
};

// V3 删除定时器，可使用闹钟代替，需要定期执行的任务可自行在回调中设置下次调用的闹钟或者直接使用拓展组件中的 task 组件
#if 0
// 定时器
struct ltx_Timer_stu {
    TickType_t tick_counts; // tick counts 会在每个 tick 减一，当减为 0 时，会赋值为 tick reload，并且发布 Topic
    TickType_t tick_reload; // 重载值/周期，赋值为 0 则相当于设置为 TickType_t 的最大值

    struct ltx_Topic_stu topic; // 定时器触发后会给所有订阅者发送通知

    struct ltx_Timer_stu *prev;
    struct ltx_Timer_stu *next;
};
#endif

// 闹钟
struct ltx_Alarm_stu {
    TickType_t diff_tick; // 闹钟间相对时间差，表示距离链表前一个节点的倒计时，变为零时发布话题通知前台调用订阅者回调函数

    struct ltx_Topic_stu topic; // 闹钟触发后会给所有订阅者发送通知
    
    struct ltx_Alarm_stu *prev;
    struct ltx_Alarm_stu *next;
};

#if 0
void ltx_Timer_add(struct ltx_Timer_stu *timer);
void ltx_Timer_remove(struct ltx_Timer_stu *timer);
#endif

void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down);
void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm);

void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Topic_unsubscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Topic_publish(struct ltx_Topic_stu *topic);

// 系统嘀嗒，由 systick/硬件定时器 中断服务函数调用
void ltx_Sys_tick_tack(void);
// 获取系统自开机以来的 tick 计数，如果开了 tickless，那么要用这个替换掉其他库的获取 tick 的函数
TickType_t ltx_Sys_get_tick(void);

// 调度器，一般放在 main 函数运行，开启空闲任务功能则放在 PendSV 之类的最低优先级的软中断里
void ltx_Sys_scheduler(void);

#ifdef ltx_cfg_USE_IDLE_TASK
// 空闲任务，如果 ltx_Sys_scheduler 放在软中断里，那么这个才能使用并且放在 main 函数最后
void ltx_Sys_idle_task(void);
// 开启空闲任务或者 tickless 的钩子函数，用于在进入休眠前或退出休眠后执行一些低功耗相关操作，函数内部不能有耗时操作
// 最好是开了 tickless 再用
void ltx_Hook_idle_in(void);
void ltx_Hook_idle_out(void);
#endif

#endif // __LTX_H__
