/**
 * @file ltx_script.h
 * @author realTiX
 * @brief ltx 可选脚本组件，依赖 ltx_lock。用于动画或者初始化硬件之类的执行序列，可在自定义回调设置下一 step 类型实现分支结构，每个 step 间可自定义延时。步骤可等待锁或事件并设置超时时间。（状态机协程疑似
 * @version 0.4
 * @date 2025-11-25 (0.1，初步完成功能设计)
 *       2025-12-01 (0.2，增加等待事件步骤类型，重构所有内容)
 *       2025-12-05 (0.3，重构所有内容，变更步骤间逻辑)
 *       2025-12-08 (0.4，修复忘了赋值下一 step 类型的 bug)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_SCRIPT_H__
#define __LTX_SCRIPT_H__

#include "ltx.h"

// 脚本步骤类型
typedef enum {
    SC_TYPE_RUN_DELAY = 0,  // 从 该步骤开始运行或运行完 到 下一步骤开始运行 间隔一定时间
    SC_TYPE_WAIT_TOPIC,     // 等待某个事件话题发布，超时时间设为 0 将一直等待直到 TickType_t 溢出（50 来天）
    SC_TYPE_WAIT_LOCK,      // 等待某个锁释放，todo
    SC_TYPE_OVER,           // 序列结束，不再执行
} ltx_Script_step_type_e;

// 等待话题步骤回调被触发的类型
typedef enum {
    SC_TT_UNKNOWN = 0,  // 未知
    SC_TT_TIMEOUT,      // 因为超时而被触发
    SC_TT_TOPIC,        // 因为话题发布而被触发
} ltx_Script_triger_type_e;

// 脚本结构体
struct ltx_Script_stu {
    uint32_t step_now; // 当前执行到的步骤，1 起，下一步骤编号需要用户在自定义回调里更新，不能设置为 0
    ltx_Script_step_type_e next_step_type; // 下一步骤的类型
    ltx_Script_triger_type_e triger_type; // 等待话题步骤被触发的原因

    void (*callback)(struct ltx_Script_stu *script); // 自定义回调

    struct ltx_Topic_stu *topic_wait_for; // 某一步骤所等待事件的话题指针
    struct ltx_Topic_subscriber_stu subscriber_wait_for; // 管理脚本等待事件的订阅者
    struct ltx_Alarm_stu alarm_next_run; // 管理脚本下次运行/超时时间的闹钟
};

int ltx_Script_init(struct ltx_Script_stu *script, void (*callback)(struct ltx_Script_stu *), ltx_Script_step_type_e step_type, TickType_t delay_ticks, struct ltx_Topic_stu *topic_wait_for);

int ltx_Script_set_next_step(struct ltx_Script_stu *script, uint32_t step_next, ltx_Script_step_type_e step_type, TickType_t delay_ticks, struct ltx_Topic_stu *topic_wait_for);

ltx_Script_triger_type_e ltx_Script_get_triger_type(struct ltx_Script_stu *script);

void ltx_Script_pause(struct ltx_Script_stu *script);
void ltx_Script_resume(struct ltx_Script_stu *script);

#endif // __LTX_SCRIPT_H__
