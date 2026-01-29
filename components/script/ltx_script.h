/**
 * @file ltx_script.h
 * @author realTiX
 * @brief ltx 可选脚本组件。用于动画或者初始化硬件之类的执行序列，可在自定义回调设置下一 step 类型实现分支结构，每个 step 间可自定义延时。步骤可等待锁或事件并设置超时时间。（状态机协程疑似
 * @version 3.0
 * @date 2025-11-25 (0.1，初步完成功能设计)
 *       2025-12-01 (0.2，增加等待事件步骤类型，重构所有内容)
 *       2025-12-05 (0.3，重构所有内容，变更步骤间逻辑)
 *       2025-12-08 (0.4，修复忘了赋值下一 step 类型的 bug)
 * 
 *       2026-01-13 (2.0，适配 ltx V2，优化使用逻辑；增加私有数据指针，便于多实例；增加脚本复位触发类型，便于脚本内部释放资源)
 * 
 *       2026-01-28 (3.0，适配 ltx V3，适配 ltx V3 的闹钟改动，修改 api，优化使用逻辑，且便于适配 V3 闹钟)
 *       2026-01-28 (3.1，修复 reset 脚本 api 的潜在错误操作)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_SCRIPT_H__
#define __LTX_SCRIPT_H__

#include "ltx.h"

// 脚本步骤类型
typedef enum {
    SC_STEP_WAIT_DELAY = 0, // 从 该步骤开始运行或运行完 到 下一步骤开始运行 间隔一定时间
    SC_STEP_WAIT_TOPIC,     // 等待某个事件话题发布，超时时间设为 0 将一直等待直到 TickType_t 溢出（50 来天）
    // SC_STEP_WAIT_LOCK,   // 等待某个锁释放，todo
    SC_STEP_OVER,           // 序列结束，不再执行
} ltx_Script_step_type_e;

// 等待话题步骤回调被触发的类型
typedef enum {
    SC_TRIGER_RESET = -1,   // 脚本重置，调用 ltx_Script_reset 会设置为此状态并调用一次脚本回调，用户可在脚本内识别到此状态后在脚本内做释放资源等操作，而无需外部插手
    SC_TRIGER_UNKNOWN = 0,  // 未知
    SC_TRIGER_TIMEOUT,      // 因为超时而被触发
    SC_TRIGER_TOPIC,        // 因为话题发布而被触发
} ltx_Script_triger_type_e;

// 脚本结构体
struct ltx_Script_stu {
    uint32_t step_now; // 当前执行到的步骤，0 起，下一步骤编号需要用户在自定义回调里更新
    ltx_Script_step_type_e next_step_type;                  // 下一步骤的类型
    ltx_Script_triger_type_e triger_type;                   // 等待话题步骤被触发的原因

    void (*callback)(struct ltx_Script_stu *script);        // 用户自定义回调

    struct ltx_Topic_stu *topic_wait_for;                   // 某一步骤所等待事件的话题指针
    struct ltx_Topic_subscriber_stu subscriber_topic;       // 管理脚本等待事件的订阅者
    struct ltx_Alarm_stu alarm_next_run;                    // 管理脚本下次运行/超时时间的闹钟
    struct ltx_Topic_subscriber_stu subscriber_alarm;       // 管理脚本闹钟事件的订阅者

    void *data;                                             // 私有数据，便于脚本多实例
};

int ltx_Script_init(struct ltx_Script_stu *script, void (*callback)(struct ltx_Script_stu *));

// 设置脚本下一步骤为延时 delay_ticks 后运行，可为 0，表示短暂出让
void ltx_Script_next_step_delay(struct ltx_Script_stu *script, uint32_t step_next, TickType_t delay_ticks);
// 设置脚本下一步骤为等待某一话题触发后运行，可设置超时时间
void ltx_Script_next_step_topic(struct ltx_Script_stu *script, uint32_t step_next, TickType_t time_out, struct ltx_Topic_stu *topic_wait_for);
// 设置脚本执行结束，不再执行
void ltx_Script_next_step_over(struct ltx_Script_stu *script);

ltx_Script_triger_type_e ltx_Script_get_triger_type(struct ltx_Script_stu *script);

void ltx_Script_pause(struct ltx_Script_stu *script);
void ltx_Script_resume(struct ltx_Script_stu *script, TickType_t delay_ticks);

void ltx_Script_reset(struct ltx_Script_stu *script);

#endif // __LTX_SCRIPT_H__
