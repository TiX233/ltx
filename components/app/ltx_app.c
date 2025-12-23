#include "ltx_app.h"
#include "ltx.h"

// 系统 APP 链表头
struct ltx_App_stu ltx_sys_app_list = {
    .next = NULL,
};

// 周期任务 API
/**
 * @brief   设置周期任务名称函数
 * @param   task: 任务结构体指针
 * @param   name: 名称
 * @retval  非 0 代表设置失败
 */
int ltx_Task_set_name(struct ltx_Task_stu *task, const char *name){
    if(name == NULL){
        return -1;
    }

    task->name = name;

    return 0;
}

/**
 * @brief   设置周期任务周期函数
 * @param   task: 任务结构体指针
 * @param   ticks: 任务周期 tick 数
 * @param   execute_immediately: 是否立即执行，非 0 表示第一次执行不等待一个周期
 * @retval  非 0 代表设置失败
 */
int ltx_Task_set_period(struct ltx_Task_stu *task, TickType_t ticks, uint8_t execute_immediately){
    if(task == NULL){
        return -1;
    }

    task->timer.tick_reload = ticks;
    if(execute_immediately){
        task->timer.tick_counts = 1;
    }else {
        task->timer.tick_counts = ticks;
    }

    return 0;
}

/**
 * @brief   设置周期任务回调函数
 * @param   task: 任务结构体指针
 * @param   callback_func: 任务回调函数指针
 * @retval  非 0 代表设置失败
 */
int ltx_Task_set_callback(struct ltx_Task_stu *task, void (*callback_func)(void *param)){
    if(task == NULL){
        return -1;
    }

    if(callback_func != NULL){
        task->subscriber.callback_func = callback_func;
    }else {
        return -2;
    }

    return 0;
}

/**
 * @brief   注册周期任务函数，默认暂停
 * @param   task: 任务结构体指针
 * @param   app: 任务所需添加到的应用的指针，为 NULL 则表示不归属任何 app，仅添加到 ltx，任务依然可以正常运行
 * @retval  非 0 代表失败
 */
int ltx_Task_init(struct ltx_Task_stu *task, struct ltx_App_stu *app){
    if(task == NULL){
        return -1;
    }

    task->topic.flag = 0;
    task->topic.subscriber = NULL;
    task->topic.next = NULL;

    task->timer.topic = &(task->topic);
    task->timer.next = NULL;

    task->subscriber.next = NULL;

    ltx_Topic_add(&(task->topic));
    ltx_Topic_subscribe(&(task->topic), &(task->subscriber));
    // ltx_Timer_add(&(task->timer));

    if(app == NULL){ // 未传递 app 参数，则此任务不添加到任何 app
        goto ltx_Task_init_over;
    }

    // 添加到 app 的任务列表便于管理
    struct ltx_Task_stu **pTask = &(app->task_list);
    while((*pTask) != NULL){
        pTask = &((*pTask)->next);
    }
    *pTask = task;
    task->next = NULL;


ltx_Task_init_over:
    task->is_initialized = 1;
    task->status = ltx_Task_status_pause;

    return 0;
}

/**
 * @brief   暂停周期任务函数
 * @param   task: 任务结构体指针
 */
int ltx_Task_pause(struct ltx_Task_stu *task){
    ltx_Timer_remove(&(task->timer));
    task->status = ltx_Task_status_pause;

    return 0;
}

/**
 * @brief   继续周期任务函数
 * @param   task: 任务结构体指针
 */
int ltx_Task_resume(struct ltx_Task_stu *task){
    ltx_Timer_add(&(task->timer));
    task->status = ltx_Task_status_running;

    return 0;
}

/**
 * @brief   删除周期任务函数
 * @param   task: 任务结构体指针
 */
int ltx_Task_destroy(struct ltx_Task_stu *task){
    ltx_Timer_remove(&(task->timer));

    // free...

    task->is_initialized = 0;
    task->status = ltx_Task_status_pause;

    return 0;
}


// 应用程序 API
int ltx_App_init(struct ltx_App_stu *app){
    if(app == NULL){
        return -1;
    }

    if(app->name == NULL){
        return -2;
    }

    // 执行用户自定义内容
    if(app->init != NULL){
        if(app->init(app)){
            return -9;
        }
    }else {
        return -3;
    }

    // 添加到管理链表
    // ? 怎么这样写的
    if(app->next != NULL){ // 已经在列表中
        goto ltx_App_init_over;
    }

    struct ltx_App_stu **pApp = &(ltx_sys_app_list.next);
    while((*pApp) != NULL){
        pApp = &((*pApp)->next);
    }
    *pApp = app;
    app->next = NULL;

ltx_App_init_over:
    // 初始化完成
    app->is_initialized = 1;
    app->status = ltx_App_status_pause;

    return 0;
}

int ltx_App_pause(struct ltx_App_stu *app){
    if(!app->is_initialized){
        return -2;
    }

    // 暂停所有任务
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_pause(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    if(app->pause){
        app->pause(app);
    }else {
        return -1;
    }

    app->status = ltx_App_status_pause;

    return 0;
}

int ltx_App_resume(struct ltx_App_stu *app){
    if(!app->is_initialized){
        return -2;
    }

    // 运行所有任务
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_resume(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    if(app->resume){
        app->resume(app);
    }else {
        return -1;
    }

    app->status = ltx_App_status_running;

    return 0;
}

int ltx_App_destroy(struct ltx_App_stu *app){
    
    struct ltx_App_stu *pApp = ltx_sys_app_list.next;

    if(pApp == NULL){
        goto ltx_App_destroy_over;
    }

    if(pApp == app){
        ltx_sys_app_list.next = app->next;
        app->next = NULL;

        goto ltx_App_destroy_over;
    }

    while(pApp->next != app && pApp->next != NULL){
        pApp = pApp->next;
    }

    if(pApp->next == app){
        pApp->next = app->next;
        app->next = NULL;
    }
    
ltx_App_destroy_over:
    app->is_initialized = 0;
    app->status = ltx_App_status_pause;

    // 销毁所有任务
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_destroy(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    if(app->destroy){
        app->destroy(app);
    }else {
        return -1;
    }

    return 0;
}
