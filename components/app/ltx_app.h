/**
 * @file ltx_app.h
 * @author realTiX
 * @brief 基于 ltx.h 的简易应用程序框架。todo：直接管理脚本能力
 * @version 2.0
 * @date 2025-10-09 (0.1, 初步完成)
 *       2025-11-16 (0.2, 更正部分注释)
 *       2025-11-17 (0.3, 为任务与应用添加列表与名字，便于管理；添加任务设置名称 api；部分 api 有变动)
 *       2025-11-19 (0.4, 增加添加 app 时检查是否已添加过)
 *       2025-11-19 (0.5, 将命名从 rtx 改为 ltx，避免重名)
 *       2025-12-01 (0.6, 更正部分注释)
 *       2025-12-03 (0.7, 补充初始化错误返回)
 *       2026-01-12 (2.0, 重构，适配 ltx V2)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_APP_H__
#define __LTX_APP_H__

#include "ltx.h"

#define _LTX_APP_CONFIG(app_name,init_cb,pause_cb,resume_cb,destroy_cb)         {.is_initialized = 0,\
                                                                                .status = 0,\
                                                                                .name = app_name,\
                                                                                .init = init_cb,\
                                                                                .pause = pause_cb,\
                                                                                .resume = resume_cb,\
                                                                                .destroy = destroy_cb,\
                                                                                .task_list = NULL,\
                                                                                .next = NULL};

// 周期任务运行状态枚举
typedef enum {
    ltx_Task_status_pause = 0,
    ltx_Task_status_running = 1,
} ltx_Task_status_e;

// 周期任务结构体
struct ltx_Task_stu {
    uint8_t is_initialized;
    uint8_t status;
    const char *name;

    struct ltx_Timer_stu timer;
    struct ltx_Topic_subscriber_stu subscriber;

    struct ltx_Task_stu *next;

    // void *private_data;
};

// 程序运行状态枚举
typedef enum {
    ltx_App_status_pause = 0,
    ltx_App_status_running = 1,
} ltx_App_status_e;

// 应用程序结构体
struct ltx_App_stu {
    uint8_t is_initialized;
    uint8_t status;
    const char *name;

    int (*init)(struct ltx_App_stu *app);
    int (*pause)(struct ltx_App_stu *app);
    int (*resume)(struct ltx_App_stu *app);
    int (*destroy)(struct ltx_App_stu *app);

    struct ltx_Task_stu *task_list; // 暂时只支持直接管理 task，脚本待办
    
    struct ltx_App_stu *next; // 初始不为 NULL 的话则该 app 无法添加进列表
};

// 应用列表
extern struct ltx_App_stu ltx_sys_app_list;

// 周期任务 API
int ltx_Task_init(struct ltx_Task_stu *task, void (*callback_func)(void *param), TickType_t period, TickType_t execute_delay);
int ltx_Task_add_to_app(struct ltx_Task_stu *task, struct ltx_App_stu *app, const char *task_name); // 周期任务可选择加入或者不加入某个 app，不加入依然可以正常运行，只是不方便管理
// 如果 task 被加入到了某个 app，那么 pause 和 resume 就不用手动调用，调用 app api 的时候会帮你调用
void ltx_Task_pause(struct ltx_Task_stu *task);
void ltx_Task_resume(struct ltx_Task_stu *task);
void ltx_Task_destroy(struct ltx_Task_stu *task, struct ltx_App_stu *app);

// 应用程序 API
int ltx_App_init(struct ltx_App_stu *app);
int ltx_App_pause(struct ltx_App_stu *app);
int ltx_App_resume(struct ltx_App_stu *app);
int ltx_App_destroy(struct ltx_App_stu *app);

#endif // __LTX_APP_H__
