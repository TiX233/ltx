#include "ltx_event_group.h"

void _ltx_Event_alarm_cb(void *param){
    struct ltx_Event_stu *pEvent = container_of(param, struct ltx_Event_stu, alarm);

    ltx_Topic_unsubscribe(&(pEvent->topic), &(pEvent->subscriber));
    ltx_Topic_remove(&(pEvent->topic));

    // 超时标志位置 1
    pEvent->events |= 0x80000000;

    pEvent->callback(pEvent);
}

void _ltx_Event_subscriber_cb(void *param){
    struct ltx_Event_stu *pEvent = container_of(param, struct ltx_Event_stu, subscriber);

    /*
    for(uint8_t i = 0; i < 32; i ++){
        if(pEvent->events_wait_for & ((uint32_t)1 << i)){
            if(!(pEvent->events & ((uint32_t)1 << i))){ // 需要触发的事件没触发，不调用回调
                return ;
            }
        }
    }
    */

    if(((pEvent->events_wait_for) & (pEvent->events)) == (pEvent->events_wait_for)){ // 满足所有需要触发的事件
        ltx_Alarm_remove(&(pEvent->alarm));
        pEvent->alarm.flag = 0;
        
        ltx_Topic_unsubscribe(&(pEvent->topic), &(pEvent->subscriber));
        ltx_Topic_remove(&(pEvent->topic));

        pEvent->callback(pEvent);
    }

}

/**
 * @brief   初始化事件组函数
 * @param   event: 事件组对象指针
 * @param   callback: 事件组回调
 * @param   events_wait_for: 所等待的事件，最高位用来表示是否为闹钟超时触发，不能置 1，否则初始化失败
 * @param   time_out: 等待事件超时事件，置为 0 将持续等待直到 TickType_t 溢出（50 来天）
 * @retval  非 0 代表初始化失败
 */
int ltx_Event_init(struct ltx_Event_stu *event, void (*callback)(struct ltx_Event_stu *), uint32_t events_wait_for, TickType_t time_out){
    if(event == NULL || callback == NULL){
        return -1;
    }

    if(events_wait_for & 0x80000000){
        return -2;
    }

    event->alarm.flag = 0;
    event->alarm.callback_alarm = _ltx_Event_alarm_cb;
    event->alarm.next = NULL;

    event->topic.flag = 0;
    event->topic.subscriber = NULL;
    event->topic.next = NULL;
    
    event->subscriber.callback_func = _ltx_Event_subscriber_cb;
    event->subscriber.next = NULL;

    ltx_Alarm_set_count(&(event->alarm), time_out);
    ltx_Alarm_add(&(event->alarm));

    ltx_Topic_add(&(event->topic));
    ltx_Topic_subscribe(&(event->topic), &(event->subscriber));
    
    event->events = 0;
    event->events_wait_for = events_wait_for;
    event->callback = callback;

    return 0;
}

/**
 * @brief   取消已初始化但还未完成或未超时的事件组函数
 * @param   event: 事件组对象指针
 * @retval  无
 */
void ltx_Event_cancel(struct ltx_Event_stu *event){
    ltx_Alarm_remove(&(event->alarm));
    event->alarm.flag = 0;
    
    ltx_Topic_unsubscribe(&(event->topic), &(event->subscriber));
    ltx_Topic_remove(&(event->topic));
}

/**
 * @brief   事件发布函数
 * @param   event: 事件组对象指针
 * @param   events_publish: 所发布的事件，最高位用来表示是否为闹钟超时触发，不能置 1，否则可能导致误判
 * @retval  无
 */
void ltx_Event_publish(struct ltx_Event_stu *event, uint32_t events_publish){
    event->events |= events_publish;
    ltx_Topic_publish(&(event->topic));
}

/**
 * @brief   事件超时判断函数
 * @param   event: 事件组对象指针
 * @retval  非 0 代表超时
 */
int ltx_Event_is_timeout(struct ltx_Event_stu *event){
    if(event->events & 0x80000000){
        return 1;
    }

    return 0;
}
