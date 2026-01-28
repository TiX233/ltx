/**
 * @file ltx_script.h
 * @author realTiX
 * @brief ltx 可选事件组组件。目前仅支持事件与，不支持事件或
 * @version 3.0
 * @date 2025-12-03 (0.1，初步完成功能设计)
 *       2025-12-08 (0.2，修复没有对成员变量初始化的恶性 bug)
 * 
 *       2026-01-13 (2.0，重构，适配 ltx V2)
 * 
 *       2026-01-28 (3.0，未作改动，仅对其版本号，直接支持 ltx V3)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_EVENT_GROUP_H__
#define __LTX_EVENT_GROUP_H__

#include "ltx.h"

struct ltx_Event_group_stu {
    uint32_t events;            // 已触发的事件集
    uint32_t events_wait_for;   // 需要等待的事件集，最高位保留用于判断是否为超时触发

    struct ltx_Topic_stu topic;
    struct ltx_Topic_subscriber_stu subscriber;
    struct ltx_Alarm_stu alarm;
    struct ltx_Topic_subscriber_stu subscriber_alarm;

    void (*callback)(struct ltx_Event_group_stu *eventg); // 满足所有事件或者超时后会调用回调，用户可调用 ltx_Event_is_timeout 来判断是否超时导致的回调触发
};

int ltx_Event_group_init(struct ltx_Event_group_stu *eventg, void (*callback)(struct ltx_Event_group_stu *), uint32_t events_wait_for, TickType_t time_out);
void ltx_Event_group_cancel(struct ltx_Event_group_stu *eventg);
void ltx_Event_group_publish(struct ltx_Event_group_stu *eventg, uint32_t events_publish);

int ltx_Event_group_is_timeout(struct ltx_Event_group_stu *eventg);

#endif // __LTX_EVENT_GROUP_H__
