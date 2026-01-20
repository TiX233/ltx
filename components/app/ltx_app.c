#include "ltx_app.h"
#include "ltx.h"

// 系统 APP 链表头
struct ltx_App_stu ltx_sys_app_list = {.next = NULL,};

// 周期任务 API
/**
 * @brief   注册周期任务函数，默认暂停，需要调用 ltx_Task_resume 或者被 app 继续后才能正常运行
 * @param   task: 任务结构体指针
 * @param   callback_func: 任务回调函数指针
 * @param   period: 任务周期
 * @param   execute_delay: 任务首次运行延时，设置为零则立即就绪执行
 * @retval  非 0 代表失败
 */
int ltx_Task_init(struct ltx_Task_stu *task, void (*callback_func)(void *param), TickType_t period, TickType_t execute_delay){
    if(task == NULL || callback_func == NULL){
        return -1;
    }
    if(period < 1){
        return -2;
    }

    task->timer.tick_counts = execute_delay;
    task->timer.tick_reload = period;
    task->timer.prev = NULL;
    task->timer.next = NULL;

    task->timer.topic.flag_is_pending = 0;
    task->timer.topic.subscriber_head.prev = NULL;
    task->timer.topic.subscriber_head.next = &(task->subscriber);
    task->timer.topic.subscriber_tail = &(task->subscriber);
    task->timer.topic.next = NULL;


    task->subscriber.callback_func = callback_func;
    task->subscriber.prev = &(task->timer.topic.subscriber_head);
    task->subscriber.next = NULL;

    task->next = NULL;

    task->status = ltx_Task_status_pause;
    task->is_initialized = 1;

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
    ltx_Timer_remove(&(task->timer));
    task->status = ltx_Task_status_pause;

    return ;
}

/**
 * @brief   继续周期任务函数
 * @param   task: 任务结构体指针
 */
void ltx_Task_resume(struct ltx_Task_stu *task){
    if(task->timer.tick_counts == 0){ // 要求立即执行
        task->timer.tick_counts = task->timer.tick_reload;
        ltx_Topic_publish(&(task->timer.topic));
    }
    ltx_Timer_add(&(task->timer));
    task->status = ltx_Task_status_running;

    return ;
}

/**
 * @brief   删除周期任务函数
 * @param   task: 任务结构体指针
 * @param   app: app 指针，如果这个 task 没加入 app 的话，那么可以传入 NULL
 */
void ltx_Task_destroy(struct ltx_Task_stu *task, struct ltx_App_stu *app){
    ltx_Timer_remove(&(task->timer));

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
