/**
 * @file ltx.h
 * @author realTiX
 * @brief 轻量级高性能的事件驱动裸机调度框架，由闹钟和发布订阅机制构成。支持多核 SMP，支持空闲休眠 tickless。
 * @version 4.1
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
 *       2026-01-29 (3.1, 修复系统嘀嗒 api 对闹钟链表的错误操作导致无法正常使用)
 *       2026-01-30 (3.2, 初步修复 tickless 不可正常使用的 bug；目前不开启补偿暂时没出现问题，开启后小概率会有 systick 提前触发弹出闹钟的 bug)
 *       2026-07-07 (3.3, 将 container_of 指针改为 uintptr_t 兼容 64 位设备)
 *       2026-07-17 (3.4, 增加 ltx_Topic_publish_high_priority API，提供高优先级事件发布能力；删除遗留 timer 代码)
 *       2026-07-20 (3.5, 优化 ltx_Topic_publish_high_priority，省略不必要的启动调度信号，提高效率)
 * 
 *       2026-09-08 (4.0, 重构，支持同构多核 SMP 调度，自动负载均衡，预留回调前临界区内钩子，用于避免 ctx 回调被多核重入；
 *                              删除话题内订阅者链表尾指针，取消订阅不再需要提供 topic 指针，初始化 topic 与 alarm 也会更简单；
 *                              将启动调度信号都转移到临界区外，避免用户可能会在自定义调度信号内部重新进入临界区导致提前释放或者死锁；
 *                              tickless 不再由调度器直接操作硬件定时器实现，调度器只返回下次唤醒时刻，由外部在唤醒时机到达后触发调度信号恢复调度器，
 *                              为时间戳调度做准备)
 *       2026-09-10 (4.1, 完成时间戳调度；
 *                              topic 不再提供取消标志位，避免取消后内存释放却依然保留在事件队列内，
 *                              移除已触发闹钟时不再是标志位而是从链表移除 topic，因为闹钟触发后立即移除概率太小所以这个问题此前没有暴露出来)
 * 
 * @copyright Copyright (c) 2025-2026, realTiX
 * @license Apache-2.0
 * 
 * SPDX-FileCopyrightText: 2025-2026 realTiX
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LTX_H__
#define __LTX_H__

// #include "main.h"
#include "ltx_config.h"

// 从结构体成员计算结构体指针
// 好像不能直接用 linux 里的，从 rtthread 里偷一个
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (uintptr_t)(&((type *)0)->member)))

// 组件结构体初始化默认参数
#define _LTX_TOPIC_DEAFULT_CONFIG()                     {0}
#define _LTX_SUBSCRIBER_DEAFULT_CONFIG(callback)        {.callback_func = callback, .prev = NULL, .next = NULL}
#define _LTX_ALARM_DEAFULT_CONFIG()                     {.diff_tick = 0, .topic = _LTX_TOPIC_DEAFULT_CONFIG(), .prev = NULL, .next = NULL}

// 话题订阅者
struct ltx_Topic_subscriber_stu {
    // 多核心/多线程 下，如果订阅者回调内释放了 topic 的内存，那么用户需要返回 1，否则返回 0。单核心/单线程 下返回值无效
    uint8_t (*callback_func)(void *param);

    struct ltx_Topic_subscriber_stu *prev;
    struct ltx_Topic_subscriber_stu *next;
};

// 话题
struct ltx_Topic_stu {
    #if (ltx_cfg_CORE_NUM > 1)
    // 多核下，可能某个话题已经被弹出，此时被某一个核心处理回调中
    // 但是再一次被推入事件队列，那么另一个核心可能也会弹出并处理它，导致回调重入，可能出现风险
    // 所以增加标志位标记它是否被某个核执行回调中，如果是，那么绕过它弹出下一个，知道它标志位被清除
    uint8_t flag_is_occupyed;
    #endif

    struct ltx_Topic_subscriber_stu subscriber_head;

    struct ltx_Topic_stu *next;
};

// 闹钟
struct ltx_Alarm_stu {
    TickType_t diff_tick; // 闹钟间相对时间差，表示距离链表前一个节点的倒计时，时间到会发布话题通知前台调用订阅者回调函数

    struct ltx_Topic_stu topic; // 闹钟触发后会给所有订阅者发送通知
    
    struct ltx_Alarm_stu *prev;
    struct ltx_Alarm_stu *next;
};

// 闹钟
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down);
void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm);
uint8_t __ltx_MC_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down, TickType_t now_sys_tick);
void __ltx_MC_Alarm_remove(struct ltx_Alarm_stu *alarm);

// 话题
void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Topic_unsubscribe(struct ltx_Topic_subscriber_stu *subscriber);
// 发布话题，默认版本。将事件对象推入事件队列最末尾
void ltx_Topic_publish(struct ltx_Topic_stu *topic);
// 发布话题，高优先级版本。将事件对象插队事件队列头，事件可以更快得到处理
void ltx_Topic_publish_high_priority(struct ltx_Topic_stu *topic);

// 调度器，一般放在 main 函数运行
void ltx_Sys_scheduler(uint8_t core_id);

#endif // __LTX_H__
