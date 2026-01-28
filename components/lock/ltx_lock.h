/**
 * @file ltx_lock.h
 * @author realTiX
 * @brief ltx 可选锁组件。可设置锁超时时间，超时后会调用用户自定义超时回调进行资源解锁。裸机情况下似乎没什么用，感觉只能用来当一个配置起来更简单的闹钟，毕竟 V2 的闹钟是真难配啊。用来中断按键消抖也许不错？
 * @version 3.0
 * @date 2025-12-08 (0.1，初步完成功能设计)
 * 
 *       2026-01-13 (2.0，重构，适配 ltx V2；似乎多个消费者同时订阅一个锁释放会有问题，如果第一个订阅者获取后占有了锁那么第二个订阅者被调用时锁还是被占有的，因为话题订阅者回调是顺序调用的)
 *       2026-01-23 (2.1，修复通用回调获取父结构体选错成员变量导致无法正常运行的 bug；优化释放锁，直接取消闹钟而不是等主循环去取消)
 * 
 *       2026-01-28 (3.0，删除遗留注释代码；逻辑未改动，仅跟上 V3 版本号)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_LOCK_H__
#define __LTX_LOCK_H__

#include "ltx.h"

struct ltx_Lock_stu {
    uint8_t flag_is_locked;                                 // 锁占有标志位

    struct ltx_Topic_stu topic_lock_release;                // 锁释放话题

    struct ltx_Alarm_stu alarm_time_out;                    // 锁超时闹钟
    struct ltx_Topic_subscriber_stu subscriber_alarm;       // 锁超时订阅者

    void (*timeout_callback)(struct ltx_Lock_stu *lock);    // 锁超时回调，用户可在这里强制释放资源解锁
};

void ltx_Lock_init(struct ltx_Lock_stu *lock, void (*timeout_callback)(struct ltx_Lock_stu *));
void ltx_Lock_locked(struct ltx_Lock_stu *lock, TickType_t timeout);
void ltx_Lock_release(struct ltx_Lock_stu *lock);

void ltx_Lock_subscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Lock_unsubscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber);
uint8_t ltx_Lock_is_locked(struct ltx_Lock_stu *lock);

#endif // __LTX_LOCK_H__
