#include "ltx_app.h"
#include "ltx.h"

// 系统 APP 链表头
struct ltx_App_stu ltx_sys_app_list = {.next = NULL,};

// 周期任务通用闹钟回调
void _ltx_Task_alarm_cb(void *param){
    struct ltx_Task_stu *pTask = container_of(param, struct ltx_Task_stu, subscriber);

    // 设置闹钟为重载值
    ltx_Alarm_add(&(pTask->alarm), pTask->tick_reload);
    pTask->tick_execute = pTask->tick_reload + ltx_Sys_get_tick();

    // 运行用户回调
    pTask->task_callback(pTask);
}

// 周期任务 API
/**
 * @brief   注册周期任务函数，默认暂停，需要调用 ltx_Task_resume 或者被 app 继续后才能正常运行
 * @param   task: 任务结构体指针
 * @param   callback_func: 任务回调函数指针
 * @param   period: 任务周期
 * @param   execute_delay: 任务首次运行延时，设置为零则立即就绪执行
 * @retval  非 0 代表失败
 */
int ltx_Task_init(struct ltx_Task_stu *task, void (*callback_func)(struct ltx_Task_stu *task), TickType_t period, TickType_t execute_delay){
    if(task == NULL || callback_func == NULL){
        return -1;
    }
    if(period < 1){
        return -2;
    }

    task->tick_reload = period;
    // task->tick_execute = execute_delay + ltx_Sys_get_tick(); // 可能会溢出
    task->tick_execute = execute_delay;

    task->alarm.diff_tick = 0;

    task->alarm.topic.flag_is_pending = 0;
    task->alarm.topic.subscriber_head.prev = NULL;
    task->alarm.topic.subscriber_head.next = &(task->subscriber);
    task->alarm.topic.subscriber_tail = &(task->subscriber);
    task->alarm.topic.next = NULL;

    task->alarm.prev = NULL;
    task->alarm.next = NULL;

    task->subscriber.callback_func = _ltx_Task_alarm_cb;
    task->subscriber.prev = &(task->alarm.topic.subscriber_head);
    task->subscriber.next = NULL;

    task->task_callback = callback_func;

    task->next = NULL;

    task->status = ltx_Task_status_pause;
    task->is_initialized = 2;

    return 0;
}

/**
 * @brief   周期任务可选择加入某个 app，不加入依然可以正常运行，只是不方便管理
 * @param   task: 任务结构体指针
 * @param   app: 所要加入的 app 指针
 * @param   task_name: task 的名字
 * @retval  非 0 代表失败
 */
int ltx_Task_add_to_app(struct ltx_Task_stu *task, struct ltx_App_stu *app, const char *task_name){
    if(task == NULL || app == NULL || task_name == NULL){
        return -1;
    }

    // 已经存在于某个 app 中。。。粗略判断
    if(task->next != NULL){
        return -2;
    }

    if(!task->is_initialized){ // 任务未经过初始化
        return -3;
    }

    task->name = task_name;

    // 添加到 app 的任务列表
    struct ltx_Task_stu **pTask = &(app->task_list);
    while((*pTask) != NULL){
        pTask = &((*pTask)->next);
    }
    *pTask = task;
    task->next = NULL;

    return 0;
}

/**
 * @brief   暂停周期任务函数
 * @param   task: 任务结构体指针
 */
void ltx_Task_pause(struct ltx_Task_stu *task){
    if(!task->is_initialized){ // 未经过初始化的任务，不予处理
        return ;
    }

    if(task->status == ltx_Task_status_pause){ // 已经暂停了的任务，不处理
        return ;
    }

    // 记录距离下次执行的时间差并暂停
    task->tick_execute -= ltx_Sys_get_tick(); // 可能有风险

    ltx_Alarm_remove(&(task->alarm));
    task->status = ltx_Task_status_pause;

    return ;
}

/**
 * @brief   继续周期任务函数
 * @param   task: 任务结构体指针
 */
void ltx_Task_resume(struct ltx_Task_stu *task){
    if(!task->is_initialized){ // 未经过初始化的任务，不予处理
        return ;
    }

    if(task->is_initialized == 2){ // 刚初始化的任务
        if(task->tick_execute){
            ltx_Alarm_add(&(task->alarm), task->tick_execute);
            task->tick_execute = ltx_Sys_get_tick() + task->tick_reload; // 计算下次执行的绝对时间点
        }else { // 要求立即执行
            ltx_Topic_publish(&(task->alarm.topic));
        }

        task->is_initialized = 1;
        task->status = ltx_Task_status_running;

        return ;
    }

    if(task->status == ltx_Task_status_running){ // 已经在运行态的任务，不处理
        return ;
    }

    // 暂停前距离下次执行的时间设置为下次执行时间
    if(task->tick_execute == 0){ // 要求立即执行
        ltx_Topic_publish(&(task->alarm.topic));
    }else {
        ltx_Alarm_add(&(task->alarm), task->tick_execute);
        task->tick_execute += ltx_Sys_get_tick(); // 计算下次执行的绝对时间点
    }
    task->status = ltx_Task_status_running;

    return ;
}

/**
 * @brief   删除周期任务函数
 * @param   task: 任务结构体指针
 * @param   app: app 指针，如果这个 task 没加入 app 的话，那么可以传入 NULL
 */
void ltx_Task_destroy(struct ltx_Task_stu *task, struct ltx_App_stu *app){
    ltx_Alarm_remove(&(task->alarm));

    task->is_initialized = 0;
    task->status = ltx_Task_status_pause;

    // 从 app 的链表删除，O(n)
    if(app == NULL){
        return ;
    }
    struct ltx_Task_stu *pTask = app->task_list;
    if(pTask == NULL){ // app 的 task 链表里没任何 task
        return ;
    }
    if(pTask == task){
        pTask = task->next;
        task->next = NULL;

        return ;
    }
    while(pTask->next != NULL){
        if(pTask->next == task){
            pTask->next = task->next;
            task->next = NULL;

            return ;
        }
    }

    return ;
}


// 应用程序 API
int ltx_App_init(struct ltx_App_stu *app){
    if(app == NULL){
        return -1;
    }

    if(app->name == NULL){
        return -2;
    }

    if(app->init == NULL || app->pause == NULL || app->resume == NULL || app->destroy == NULL){
        return -3;
    }

    // 执行用户自定义内容
    if(app->init(app)){
        return -9;
    }

    // 添加到管理链表
    if(app->next != NULL){ // 已经在列表中
        goto ltx_App_init_over;
    }

    // O(n)
    struct ltx_App_stu **pApp = &(ltx_sys_app_list.next);
    while((*pApp) != NULL){
        pApp = &((*pApp)->next);
    }
    *pApp = app;
    app->next = NULL;

ltx_App_init_over:
    // 初始化完成
    app->status = ltx_App_status_pause;
    app->is_initialized = 1;

    return 0;
}

int ltx_App_pause(struct ltx_App_stu *app){
    if(!app->is_initialized){
        return -1;
    }

    // 暂停所有任务
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_pause(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    app->pause(app);
    app->status = ltx_App_status_pause;

    return 0;
}

int ltx_App_resume(struct ltx_App_stu *app){
    if(!app->is_initialized){
        return -1;
    }

    // 运行所有任务
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_resume(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    app->resume(app);
    app->status = ltx_App_status_running;

    return 0;
}

int ltx_App_destroy(struct ltx_App_stu *app){
    if(!app->is_initialized){
        return 0;
    }
    
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

    // 暂停所有任务，而非销毁，销毁交给用户，不然不好处理释放动态申请的内存
    struct ltx_Task_stu *pTask = app->task_list;
    while(pTask != NULL){
        ltx_Task_pause(pTask);
        pTask = pTask->next;
    }

    // 执行用户自定义内容
    app->destroy(app);

    return 0;
}
