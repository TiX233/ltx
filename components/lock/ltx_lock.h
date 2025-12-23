/**
 * @file ltx_lock.h
 * @author realTiX
 * @brief ltx 可选锁组件。一般搭配脚本组件使用，可设置锁超时时间，超时后会调用用户自定义超时回调进行资源解锁
 * @version 0.1
 * @date 2025-12-08 (0.1，初步完成功能设计)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_LOCK_H__
#define __LTX_LOCK_H__

#include "ltx.h"

struct ltx_Lock_stu {
    uint8_t lock;

    struct ltx_Topic_stu topic_lock_release;

    struct ltx_Alarm_stu _alarm;
    // struct ltx_Topic_stu _topic;
    // struct ltx_Topic_subscriber_stu _subscriber;

    void (*timeout_callback)(struct ltx_Lock_stu *lock);
};

void ltx_Lock_init(struct ltx_Lock_stu *lock, void (*timeout_callback)(struct ltx_Lock_stu *));
void ltx_Lock_on(struct ltx_Lock_stu *lock, TickType_t timeout);
void ltx_Lock_off(struct ltx_Lock_stu *lock);

void ltx_Lock_subscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber);
void ltx_Lock_unsubscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber);
uint8_t ltx_Lock_is_on(struct ltx_Lock_stu *lock);

#endif // __LTX_LOCK_H__
