/**
 * @file ltx.h
 * @author realTiX
 * @brief 简易的裸机调度框架，主要由定时器、闹钟和发布订阅机制构成。1tick = 1ms
 * @version 0.13
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
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __LTX_H__
#define __LTX_H__

#include "main.h"

// 好像不能直接用 linux 里的，从 rtthread 里偷一个
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))


typedef uint32_t TickType_t;
typedef uint16_t UsType_t;

struct ltx_Topic_subscriber_stu {

    void (*callback_func)(void *param);

    struct ltx_Topic_subscriber_stu *next;
};

struct ltx_Topic_stu {
    uint8_t flag;
    struct ltx_Topic_subscriber_stu *subscriber;

    struct ltx_Topic_stu *next;
};

struct ltx_Timer_stu {
    
    TickType_t tick_counts; // tick counts 会在每个 tick 减一，当减为 0 时，会赋值为 tick reload，并且发布 Topic
    TickType_t tick_reload; // 重载值/周期，赋值为 0 则相当于设置为 TickType_t 的最大值

    struct ltx_Topic_stu *topic; // 定时器触发后会给所有订阅者发送通知

    struct ltx_Timer_stu *next;
};

struct ltx_Alarm_stu {
    uint8_t flag; // 响铃标志位
    TickType_t tick_count_down; // 闹钟倒计时，变为零时 flag ++，以通知前台调用回调函数

    void (*callback_alarm)(void *param); // 闹钟到时间后会调用相应回调函数
    
    struct ltx_Alarm_stu *next;
};

void ltx_Timer_add(struct ltx_Timer_stu *timer);
void ltx_Timer_remove(struct ltx_Timer_stu *timer);

void ltx_Alarm_add(struct ltx_Alarm_stu *alarm);
void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm);
void ltx_Alarm_set_count(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down);

void ltx_Topic_add(struct ltx_Topic_stu *topic);
void ltx_Topic_remove(struct ltx_Topic_stu *topic);
void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Topic_unsubscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Topic_publish(struct ltx_Topic_stu *topic);

void ltx_Sys_tick_tack(void);
TickType_t ltx_Sys_get_tick(void);
static inline UsType_t ltx_Sys_get_us(void){
    uint32_t v = SysTick->VAL;
    v *= 1000;
    v /= SysTick->LOAD;

    return 1000 - v;
}

void ltx_Sys_scheduler(void);
void ltx_Sys_schedule_start(void);
void ltx_Sys_schedule_stop(void);

#endif // __LTX_H__
