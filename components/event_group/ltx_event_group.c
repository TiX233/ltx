#include "ltx_event_group.h"

// 事件组通用超时回调
void _ltx_Event_group_alarm_cb(void *param){
    struct ltx_Event_group_stu *pEventg = container_of(param, struct ltx_Event_group_stu, subscriber_alarm);

    ltx_Topic_unsubscribe(&(pEventg->topic), &(pEventg->subscriber));

    // 超时标志位置 1
    pEventg->events |= 0x80000000;

    pEventg->callback(pEventg);
}

// 事件组通用发布事件回调
void _ltx_Event_group_subscriber_cb(void *param){
    struct ltx_Event_group_stu *pEventg = container_of(param, struct ltx_Event_group_stu, subscriber);

    if(((pEventg->events_wait_for) & (pEventg->events)) == (pEventg->events_wait_for)){ // 满足所有需要触发的事件
        ltx_Alarm_remove(&(pEventg->alarm));
        
        ltx_Topic_unsubscribe(&(pEventg->topic), &(pEventg->subscriber));

        pEventg->callback(pEventg);
    }
}

/**
 * @brief   初始化事件组函数
 * @param   eventg: 事件组对象指针
 * @param   callback: 事件组回调，等待事件完成或者超时都会调用
 * @param   events_wait_for: 所等待的事件，最高位用来表示是否为闹钟超时触发，不能置 1，否则初始化失败
 * @param   time_out: 等待事件超时时间，置为 0 将持续等待直到 TickType_t 溢出（50 来天）
 * @retval  非 0 代表初始化失败
 */
int ltx_Event_group_init(struct ltx_Event_group_stu *eventg, void (*callback)(struct ltx_Event_group_stu *), uint32_t events_wait_for, TickType_t time_out){
    if(eventg == NULL || callback == NULL){
        return -1;
    }

    if(events_wait_for & 0x80000000){ // 最高位留给超时标记
        return -2;
    }

    eventg->topic.flag_is_pending = 0;
    eventg->topic.subscriber_head.prev = NULL;
    eventg->topic.subscriber_head.next = &(eventg->subscriber);
    eventg->topic.subscriber_tail = &(eventg->subscriber);
    eventg->topic.next = NULL;
    
    eventg->subscriber.callback_func = _ltx_Event_group_subscriber_cb;
    eventg->subscriber.prev = &(eventg->topic.subscriber_head);
    eventg->subscriber.next = NULL;

    // eventg->alarm.tick_count_down = time_out;
    eventg->alarm.topic.flag_is_pending = 0;
    eventg->alarm.topic.subscriber_head.prev = NULL;
    eventg->alarm.topic.subscriber_head.next = &(eventg->subscriber_alarm);
    eventg->alarm.topic.subscriber_tail = &(eventg->subscriber_alarm);
    eventg->alarm.topic.next = NULL;
    eventg->alarm.prev = NULL;
    eventg->alarm.next = NULL;

    eventg->subscriber_alarm.callback_func = _ltx_Event_group_alarm_cb;
    eventg->subscriber_alarm.prev = &(eventg->alarm.topic.subscriber_head);
    eventg->subscriber_alarm.next = NULL;
    
    eventg->events = 0;
    eventg->events_wait_for = events_wait_for;
    eventg->callback = callback;

    ltx_Alarm_add(&(eventg->alarm), time_out);

    return 0;
}

/**
 * @brief   取消已初始化但还未完成或未超时的事件组函数
 * @param   eventg: 事件组对象指针
 * @retval  无
 */
void ltx_Event_group_cancel(struct ltx_Event_group_stu *eventg){
    ltx_Alarm_remove(&(eventg->alarm));
    
    ltx_Topic_unsubscribe(&(eventg->topic), &(eventg->subscriber));
}

/**
 * @brief   事件发布函数
 * @param   eventg: 事件组对象指针
 * @param   events_publish: 所发布的事件，最高位用来表示是否为闹钟超时触发，不能置 1，否则可能导致误判
 * @retval  无
 */
void ltx_Event_group_publish(struct ltx_Event_group_stu *eventg, uint32_t events_publish){
    eventg->events |= events_publish;
    ltx_Topic_publish(&(eventg->topic));
}

/**
 * @brief   事件超时判断函数
 * @param   eventg: 事件组对象指针
 * @retval  非 0 代表超时
 */
int ltx_Event_group_is_timeout(struct ltx_Event_group_stu *eventg){
    if(eventg->events & 0x80000000){
        return 1;
    }

    return 0;
}
