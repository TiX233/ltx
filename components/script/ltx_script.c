#include "ltx_script.h"
#include "ltx.h"

// 脚本闹钟通用回调
void _ltx_Script_alarm_cb(void *param){
    struct ltx_Script_stu *pScript = container_of(param, struct ltx_Script_stu, subscriber_alarm);

    pScript->triger_type = SC_TRIGER_TIMEOUT;

    if(pScript->next_step_type == SC_STEP_WAIT_TOPIC){ // 等待事件超时
        // 取消订阅该事件
        ltx_Topic_unsubscribe(pScript->topic_wait_for, &(pScript->subscriber_topic));
    }
    // 调用回调
    pScript->callback(pScript);
}

// 脚本订阅话题通用回调
void _ltx_Script_subscriber_cb(void *param){
    struct ltx_Script_stu *pScript = container_of(param, struct ltx_Script_stu, subscriber_topic);
    // 关闭超时闹钟
    ltx_Alarm_remove(&(pScript->alarm_next_run));
    // 取消订阅该事件
    ltx_Topic_unsubscribe(pScript->topic_wait_for, &(pScript->subscriber_topic));

    pScript->triger_type = SC_TRIGER_TOPIC;

    // 调用回调
    pScript->callback(pScript);
}

/**
 * @brief   初始化脚本函数，同一脚本只能调用一次。初始化后需要用户调用一次 ltx_Script_resume 函数该脚本才会开始运行。初次运行会进入 case 0
 * @param   script: 脚本对象指针
 * @param   callback: 脚本回调
 * @param   delay_ticks: 调用 ltx_Script_resume 函数后延时 delay_ticks 后再执行首个步骤（case 0），可设置为 0，表示尽快执行
 * @retval  非 0 代表初始化失败
 */
int ltx_Script_init(struct ltx_Script_stu *script, void (*callback)(struct ltx_Script_stu *), TickType_t delay_ticks){

    if(script == NULL || callback == NULL){
        return -1;
    }

    script->step_now = 0;
    script->triger_type = SC_TRIGER_UNKNOWN;
    script->callback = callback;
    script->next_step_type = SC_STEP_WAIT_DELAY;

    script->topic_wait_for = NULL;

    script->subscriber_topic.prev = NULL;
    script->subscriber_topic.callback_func = _ltx_Script_subscriber_cb;
    script->subscriber_topic.next = NULL;

    script->alarm_next_run.prev = NULL;
    script->alarm_next_run.next = NULL;
    script->alarm_next_run.tick_count_down = delay_ticks;

    script->alarm_next_run.topic.flag_is_pending = 0;
    script->alarm_next_run.topic.next = NULL;
    script->alarm_next_run.topic.subscriber_head.prev = NULL;
    script->alarm_next_run.topic.subscriber_head.next = &(script->subscriber_alarm);
    script->alarm_next_run.topic.subscriber_tail = &(script->subscriber_alarm);

    script->subscriber_alarm.prev = &(script->alarm_next_run.topic.subscriber_head);
    script->subscriber_alarm.next = NULL;
    script->subscriber_alarm.callback_func = _ltx_Script_alarm_cb;
    
	return 0;
}

/**
 * @brief   设置脚本下一步骤为延时 delay_ticks 后运行，由用户在回调函数中调用
 * @param   script: 脚本对象指针
 * @param   next_step_type: 下一步骤编号
 * @param   delay_ticks: 延时时间（非阻塞），可为 0，表示短暂出让（具体就是把自己移到话题就绪队列尾）
 * @retval  无
 */
void ltx_Script_next_step_delay(struct ltx_Script_stu *script, uint32_t step_next, TickType_t delay_ticks){
    
    script->step_now = step_next;
    script->next_step_type = SC_STEP_WAIT_DELAY;

    if(delay_ticks == 0){ // 直接添加到就绪队列，无需等到下一个 tick
        ltx_Topic_publish(&(script->alarm_next_run.topic));
        return ;
    }
    ltx_Alarm_add(&(script->alarm_next_run), delay_ticks);

    return ;
}

/**
 * @brief   设置脚本下一执行步骤为等待某一话题触发或超时后运行，由用户在回调函数中调用
 * @param   script: 脚本对象指针
 * @param   next_step_type: 下一步骤编号
 * @param   time_out: 等待话题超时时间，设置为 0 表示一直等待直到 TickType_t 溢出
 * @param   topic_wait_for: 下一该步骤等待的事件
 * @retval  无
 */
void ltx_Script_next_step_topic(struct ltx_Script_stu *script, uint32_t step_next, TickType_t time_out, struct ltx_Topic_stu *topic_wait_for){
    
    // if(topic_wait_for == NULL){
    //     return -1;
    // }

    script->step_now = step_next;
    script->next_step_type = SC_STEP_WAIT_TOPIC;

    script->topic_wait_for = topic_wait_for;
    ltx_Topic_subscribe(topic_wait_for, &(script->subscriber_topic));

    ltx_Alarm_add(&(script->alarm_next_run), time_out);

    return ;
}

/**
 * @brief   设置脚本执行结束，不需要继续执行
 * @retval  无
 */
void ltx_Script_next_step_over(struct ltx_Script_stu *script){
    script->next_step_type = SC_STEP_OVER;
}

/**
 * @brief   获取当前步骤回调是由什么原因被调用，由用户在回调函数中调用
 * @param   script: 脚本对象指针
 * @retval  调用原因
 */
ltx_Script_triger_type_e ltx_Script_get_triger_type(struct ltx_Script_stu *script){
    return script->triger_type;
}

/**
 * @brief   暂停正在运行的脚本
 * @param   script: 脚本对象指针
 * @retval  无
 */
void ltx_Script_pause(struct ltx_Script_stu *script){
    ltx_Alarm_remove(&(script->alarm_next_run));
    
    if(script->next_step_type == SC_STEP_WAIT_TOPIC){
        ltx_Topic_unsubscribe(script->topic_wait_for, &(script->subscriber_topic));
    }
}

/**
 * @brief   恢复正在暂停的脚本
 * @param   script: 脚本对象指针
 * @retval  无
 */
void ltx_Script_resume(struct ltx_Script_stu *script){
    if(script->triger_type == SC_TRIGER_UNKNOWN){ // 第一次执行
        if(script->alarm_next_run.tick_count_down == 0){ // 启动延时时间为 0，要求尽快执行
            // script->triger_type = SC_TRIGER_TIMEOUT; // 不用管
            // 直接推入就绪队列
            ltx_Topic_publish(&(script->alarm_next_run.topic));
        }else { // 启动延时时间非 0
            ltx_Alarm_add(&(script->alarm_next_run), script->alarm_next_run.tick_count_down);
        }

        return ;
    }
    // 不是第一次执行
    ltx_Alarm_add(&(script->alarm_next_run), script->alarm_next_run.tick_count_down);
    if(script->next_step_type == SC_STEP_WAIT_TOPIC){
        ltx_Topic_subscribe(script->topic_wait_for, &(script->subscriber_topic));
    }
}

/**
 * @brief   让已经初始化的 正在运行/运行结束 的脚本从头运行，此函数会调用一次脚本回调，以便用户在内部释放资源；
 *          调用此函数后需要调用一次 ltx_Script_resume 才能在 delay_ticks 后开始脚本运行；
 *          也可用作对某个运行中脚本的 kill 操作
 * @param   script: 脚本对象指针
 * @param   delay_ticks: 调用 ltx_Script_resume 函数后延时 delay_ticks 后再执行首个步骤（case 0），可设置为 0，表示尽快执行
 * @retval  无
 */
void ltx_Script_reset(struct ltx_Script_stu *script, TickType_t delay_ticks){
    ltx_Script_pause(script);

    // 触发类型设置为复位，调用一次脚本回调，把复位信息通知给脚本状态机，让他自己去释放资源什么的，不用外部插手
    script->triger_type = SC_TRIGER_RESET;
    script->callback(script);

    script->step_now = 0;
    script->triger_type = SC_TRIGER_UNKNOWN;
    script->next_step_type = SC_STEP_WAIT_DELAY;
    script->alarm_next_run.tick_count_down = delay_ticks;
}
