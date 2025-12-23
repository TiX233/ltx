#include "ltx_script.h"
#include "ltx.h"


void _ltx_Script_alarm_cb(void *param){
    struct ltx_Script_stu *pScript = container_of(param, struct ltx_Script_stu, alarm_next_run);

    pScript->triger_type = SC_TT_TIMEOUT;

    if(pScript->next_step_type == SC_TYPE_WAIT_TOPIC){ // 等待事件超时
        // 取消订阅该事件
        ltx_Topic_unsubscribe(pScript->topic_wait_for, &(pScript->subscriber_wait_for));
    }
    // 调用回调
    pScript->callback(pScript);
}

void _ltx_Script_subscriber_cb(void *param){
    struct ltx_Script_stu *pScript = container_of(param, struct ltx_Script_stu, subscriber_wait_for);
    // 关闭超时闹钟
    ltx_Alarm_remove(&(pScript->alarm_next_run));
    pScript->alarm_next_run.flag = 0;
    // 取消订阅该事件
    ltx_Topic_unsubscribe(pScript->topic_wait_for, &(pScript->subscriber_wait_for));

    pScript->triger_type = SC_TT_TOPIC;

    // 调用回调
    pScript->callback(pScript);
}

/**
 * @brief   初始化脚本函数，初始化后需要用户调用一次 ltx_Script_resume 函数该脚本才会开始运行
 * @param   script: 脚本对象指针
 * @param   callback: 脚本回调
 * @param   step_type: 第一次执行的步骤的类型
 * @param   timeout_ticks: 等待事件超时时间，仅 step_type 为 SC_TYPE_WAIT_TOPIC 有效，传入 0 则持续等待直到 TickType_t 溢出
 * @param   topic_wait_for: 该步骤等待的事件，仅 step_type 为 SC_TYPE_WAIT_TOPIC 有效，非该类型则可传入 NULL
 * @retval  非 0 代表初始化失败
 */
int ltx_Script_init(struct ltx_Script_stu *script, void (*callback)(struct ltx_Script_stu *), ltx_Script_step_type_e step_type, TickType_t timeout_ticks, struct ltx_Topic_stu *topic_wait_for){

    if(script == NULL || callback == NULL){
        return -1;
    }

    script->step_now = 0;
    script->triger_type = SC_TT_UNKNOWN;
    script->callback = callback;
    script->next_step_type = step_type;

    script->alarm_next_run.flag = 0;
    script->alarm_next_run.callback_alarm = _ltx_Script_alarm_cb;
    script->alarm_next_run.tick_count_down = timeout_ticks;
    script->alarm_next_run.next = NULL;

    script->subscriber_wait_for.callback_func = _ltx_Script_subscriber_cb;
    script->subscriber_wait_for.next = NULL;

    if(step_type == SC_TYPE_WAIT_TOPIC){
        if(topic_wait_for == NULL){
            return -2;
        }else {
            // ltx_Topic_subscribe(topic_wait_for, &(script->subscriber_wait_for));
        }
    }
    script->topic_wait_for = topic_wait_for;
	
	return 0;
}

/**
 * @brief   设置脚本下一执行步骤的函数，由用户在回调函数中调用
 * @param   script: 脚本对象指针
 * @param   next_step_type: 下一步骤编号，不可设置为 0
 * @param   step_type: 下一次执行的步骤的类型
 * @param   delay_ticks: 当前步骤与下一步骤间的延时 或（如果下一步骤类型为 SC_TYPE_WAIT_TOPIC）下一步骤超时时间 或 （如果下一步骤类型为 SC_TYPE_WAIT_LOCK）该步骤占有锁的超时时间
 * @param   topic_wait_for: 下一该步骤等待的事件，仅 step_type 为 SC_TYPE_WAIT_TOPIC 有效，非该类型则可传入 NULL
 * @retval  非 0 代表设置失败
 */
int ltx_Script_set_next_step(struct ltx_Script_stu *script, uint32_t step_next, ltx_Script_step_type_e next_step_type, TickType_t delay_ticks, struct ltx_Topic_stu *topic_wait_for){
    if(next_step_type == SC_TYPE_WAIT_TOPIC){
        if(topic_wait_for == NULL){
            return -1;
        }
    }

    script->step_now = step_next;
    script->next_step_type = next_step_type;

    switch(script->next_step_type){
        case SC_TYPE_RUN_DELAY:
            
            ltx_Alarm_set_count(&(script->alarm_next_run), delay_ticks);
            ltx_Alarm_add(&(script->alarm_next_run));
            if(delay_ticks == 0){ // 下次调度轮询立即执行，无需等到下一个 tick
                script->alarm_next_run.flag = 1;
            }

            break;

        case SC_TYPE_WAIT_TOPIC:

            script->topic_wait_for = topic_wait_for;
            ltx_Topic_subscribe(topic_wait_for, &(script->subscriber_wait_for));

            ltx_Alarm_set_count(&(script->alarm_next_run), delay_ticks);
            ltx_Alarm_add(&(script->alarm_next_run));

            break;

        case SC_TYPE_WAIT_LOCK:
            
            break;

        case SC_TYPE_OVER:

            break;

        default:

            break;
    }

    return 0;
}

/**
 * @brief   获取当前步骤回调是由什么原因被调用，由用户在回调函数中调用
 * @param   script: 脚本对象指针
 * @retval  调用原因
 */
ltx_Script_triger_type_e ltx_Script_get_triger_type(struct ltx_Script_stu *script){

    return script->triger_type;
}

void ltx_Script_pause(struct ltx_Script_stu *script){
    ltx_Alarm_remove(&(script->alarm_next_run));
    // script->alarm_next_run.flag = 0;
    if(script->next_step_type == SC_TYPE_WAIT_TOPIC){
        ltx_Topic_unsubscribe(script->topic_wait_for, &(script->subscriber_wait_for));
    }
}

void ltx_Script_resume(struct ltx_Script_stu *script){
    if(script->step_now == 0){ // 第一次执行
        if(script->next_step_type == SC_TYPE_WAIT_TOPIC){ // 第一次步骤为等待事件
            ltx_Topic_subscribe(script->topic_wait_for, &(script->subscriber_wait_for));
            ltx_Alarm_add(&(script->alarm_next_run));

            return ;
        }

        // 第一次步骤不为等待事件，直接执行
        script->step_now = 1;
        script->callback(script);

        return ;
    }
    // 不是第一次执行
    ltx_Alarm_add(&(script->alarm_next_run));
    if(script->next_step_type == SC_TYPE_WAIT_TOPIC){
        ltx_Topic_subscribe(script->topic_wait_for, &(script->subscriber_wait_for));
    }
}
