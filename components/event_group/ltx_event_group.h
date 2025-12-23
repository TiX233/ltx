/**
 * @file ltx_script.h
 * @author realTiX
 * @brief ltx 可选事件组组件。
 * @version 0.2
 * @date 2025-12-03 (0.1，初步完成功能设计)
 *       2025-12-08 (0.2，修复没有对成员变量初始化的恶性 bug)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_EVENT_GROUP_H__
#define __LTX_EVENT_GROUP_H__

#include "ltx.h"

struct ltx_Event_stu {
    uint32_t events;
    uint32_t events_wait_for; // 等待的事件，最高位保留用于判断是否为超时触发

    struct ltx_Topic_stu topic;
    struct ltx_Topic_subscriber_stu subscriber;
    struct ltx_Alarm_stu alarm;

    void (*callback)(struct ltx_Event_stu *event); // 满足所有事件或者超时后会调用回调，用户可调用 ltx_Event_is_timeout 来判断是否超时导致的回调触发
};

int ltx_Event_init(struct ltx_Event_stu *event, void (*callback)(struct ltx_Event_stu *), uint32_t events_wait_for, TickType_t time_out);
void ltx_Event_cancel(struct ltx_Event_stu *event);
void ltx_Event_publish(struct ltx_Event_stu *event, uint32_t events_publish);

int ltx_Event_is_timeout(struct ltx_Event_stu *event);

#endif // __LTX_EVENT_GROUP_H__
