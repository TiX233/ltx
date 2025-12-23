/**
 * @file ltx_app.h
 * @author realTiX
 * @brief 基于 ltx.h 的简易应用程序框架
 * @version 0.7
 * @date 2025-10-09 (0.1, 初步完成)
 *       2025-11-16 (0.2, 更正部分注释)
 *       2025-11-17 (0.3, 为任务与应用添加列表与名字，便于管理；添加任务设置名称 api；部分 api 有变动)
 *       2025-11-19 (0.4, 增加添加 app 时检查是否已添加过)
 *       2025-11-19 (0.5, 将命名从 rtx 改为 ltx，避免重名)
 *       2025-12-01 (0.6, 更正部分注释)
 *       2025-12-03 (0.7, 补充初始化错误返回)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_APP_H__
#define __LTX_APP_H__

#include "ltx.h"

// 周期任务运行状态枚举
typedef enum {
    ltx_Task_status_pause = 0,
    ltx_Task_status_running = 1,
} ltx_Task_status_e;

// 周期任务结构体
// 只有周期任务还是太狭隘了，改成事件驱动更好，反正周期任务本身也是定时器事件驱动，再说
struct ltx_Task_stu {
    uint8_t is_initialized;
    uint8_t status;
    const char *name;

    struct ltx_Topic_subscriber_stu subscriber;
    struct ltx_Topic_stu topic;
    struct ltx_Timer_stu timer;

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

    struct ltx_Task_stu *task_list;
    
    struct ltx_App_stu *next; // 初始不为 NULL 的话则该 app 无法添加进列表
};

// 应用列表
extern struct ltx_App_stu ltx_sys_app_list;

// 周期任务 API
int ltx_Task_set_name(struct ltx_Task_stu *task, const char *name);
int ltx_Task_set_period(struct ltx_Task_stu *task, TickType_t ticks, uint8_t execute_immediately);
int ltx_Task_set_callback(struct ltx_Task_stu *task, void (*callback_func)(void *param));
int ltx_Task_init(struct ltx_Task_stu *task, struct ltx_App_stu *app);
int ltx_Task_pause(struct ltx_Task_stu *task);
int ltx_Task_resume(struct ltx_Task_stu *task);
int ltx_Task_destroy(struct ltx_Task_stu *task);

// 应用程序 API
int ltx_App_init(struct ltx_App_stu *app);
int ltx_App_pause(struct ltx_App_stu *app);
int ltx_App_resume(struct ltx_App_stu *app);
int ltx_App_destroy(struct ltx_App_stu *app);

#endif // __LTX_APP_H__
