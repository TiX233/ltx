#include "ltx.h"

volatile TickType_t realTicks; // 系统时间，溢出处理 todo
uint8_t flag_schedule = 0; // 调度开关标志位

struct ltx_Topic_stu ltx_sys_topic_list = {
    .subscriber = NULL,

    .next = NULL,
};

struct ltx_Timer_stu ltx_sys_timer_list = {
    .next = NULL,
};

struct ltx_Alarm_stu ltx_sys_alarm_list = {
    .next = NULL,
};

// 定时器
void ltx_Timer_add(struct ltx_Timer_stu *timer){

    if(timer->next != NULL){ // 已经存在，不重复加入
        return ;
    }

    struct ltx_Timer_stu **pTimer = &(ltx_sys_timer_list.next);
    while((*pTimer) != NULL){
        pTimer = &((*pTimer)->next);
    }
    // 如果在这里产生中断并添加元素，那么中断添加的元素会被下面这个顶掉，待改进
    *pTimer = timer;
    timer->next = NULL;
}

void ltx_Timer_remove(struct ltx_Timer_stu *timer){
    struct ltx_Timer_stu *pTimer = ltx_sys_timer_list.next;

    if(pTimer == NULL){ // 定时器不在活跃列表中，不做操作
        return ;
    }

    if(pTimer == timer){
        ltx_sys_timer_list.next = timer->next;
        timer->next = NULL;

        return ;
    }

    while(pTimer->next != timer && pTimer->next != NULL){
        pTimer = pTimer->next;
    }

    if(pTimer->next == timer){
        pTimer->next = timer->next;
        timer->next = NULL;
    }

}

// 闹钟
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm){
    
    if(alarm->next != NULL){ // 已经存在，不重复加入
        return ;
    }

    struct ltx_Alarm_stu **pAlarm = &(ltx_sys_alarm_list.next);
    while((*pAlarm) != NULL){
        pAlarm = &((*pAlarm)->next);
    }
    // 如果在这里产生中断并添加元素，那么中断添加的元素会被下面这个顶掉，待改进
    *pAlarm = alarm;
    alarm->next = NULL;
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    struct ltx_Alarm_stu *pAlarm = ltx_sys_alarm_list.next;

    if(pAlarm == NULL){ // 闹钟不在活跃列表中，不做操作
        return ;
    }

    if(pAlarm == alarm){
        ltx_sys_alarm_list.next = alarm->next;
        alarm->next = NULL;
        alarm->flag = 0;

        return ;
    }

    while(pAlarm->next != alarm && pAlarm->next != NULL){
        pAlarm = pAlarm->next;
    }

    if(pAlarm->next == alarm){
        pAlarm->next = alarm->next;
        alarm->next = NULL;
        alarm->flag = 0;
    }

}

void ltx_Alarm_set_count(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down){
    alarm->tick_count_down = tick_count_down;
}

// 话题，感觉可以从轮询链表改为加入就绪队列，调度器只要弹出队列里的话题执行而不用每次轮询所有活跃话题的标志位，todo
void ltx_Topic_add(struct ltx_Topic_stu *topic){

    if(topic->next != NULL){ // 已经存在，不重复加入
        return ;
    }

    struct ltx_Topic_stu **pTopic = &(ltx_sys_topic_list.next);
    while((*pTopic) != NULL){
        pTopic = &((*pTopic)->next);
    }
    // 如果在这里产生中断并添加元素，那么中断添加的元素会被下面这个顶掉，待改进
    *pTopic = topic;
    topic->next = NULL;
}

void ltx_Topic_remove(struct ltx_Topic_stu *topic){
    struct ltx_Topic_stu *pTopic = ltx_sys_topic_list.next;

    if(pTopic == NULL){ // 话题没有在活动列表中，不做操作
        return ;
    }

    if(pTopic == topic){
        ltx_sys_topic_list.next = topic->next;
        topic->next = NULL;

        return ;
    }

    while(pTopic->next != topic && pTopic->next != NULL){
        pTopic = pTopic->next;
    }

    if(pTopic->next == topic){
        pTopic->next = topic->next;
        topic->next = NULL;
    }

}

void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){

    if(subscriber->next != NULL){ // 已经存在，不重复添加
        // 但是不添加额外的成员变量的话，只能遍历所有话题才能避免一个订阅者订阅多个话题，先不管
        return ;
    }
    
    struct ltx_Topic_subscriber_stu **pSub = &(topic->subscriber);
    while((*pSub) != NULL){
        pSub = &((*pSub)->next);
    }
    // 如果在这里产生中断并添加元素，那么中断添加的元素会被下面这个顶掉，待改进
    *pSub = subscriber;
    subscriber->next = NULL;
}

void ltx_Topic_unsubscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){
    struct ltx_Topic_subscriber_stu *pSub = topic->subscriber;
    
    if(pSub == NULL){ // 话题没有订阅者，不做操作
        return ;
    }

    if(pSub == subscriber){
        topic->subscriber = subscriber->next;
        subscriber->next = NULL;
        
        return ;
    }
    
    while(pSub->next != subscriber && pSub->next != NULL){
        pSub = pSub->next;
    }
    
    if(pSub->next == subscriber){
        pSub->next = subscriber->next;
        subscriber->next = NULL;
    }
}

void ltx_Topic_publish(struct ltx_Topic_stu *topic){ // 可安全地在中断中使用
    topic->flag = 1;
}

// 系统调度相关
void ltx_Sys_tick_tack(void){
    realTicks ++;
    if(!flag_schedule){
        return ;
    }

    struct ltx_Alarm_stu *pAlarm = ltx_sys_alarm_list.next;
    while(pAlarm != NULL){
        if(0 == --pAlarm->tick_count_down){
            pAlarm->flag = 1;
        }
        pAlarm = pAlarm->next;
    }

    struct ltx_Timer_stu *pTimer = ltx_sys_timer_list.next;
    while(pTimer != NULL){
        if(0 == -- pTimer->tick_counts){
            pTimer->tick_counts = pTimer->tick_reload;
            // ltx_Topic_publish(pTimer->topic);
            // 不调接口了，直接置 1 吧
            // 暂时先不加判断 topic 是否空
            pTimer->topic->flag = 1;
        }
        pTimer = pTimer->next;
    }
}

TickType_t ltx_Sys_get_tick(void){
    return realTicks;
}

void ltx_Sys_scheduler(void){
    struct ltx_Topic_stu *pTopic;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;
    struct ltx_Alarm_stu *pAlarm;
    struct ltx_Alarm_stu *pAlarm2;

    while(1){
        // 处理订阅
        pTopic = ltx_sys_topic_list.next;
        while(pTopic != NULL){
            if(pTopic->flag){
                pTopic->flag = 0;

                pSubscriber = pTopic->subscriber;
                while(pSubscriber != NULL){
                    // 加一行 next 暂存，不然如果回调里把自己取消订阅了，那么 next 就是 NULL，那后续所有订阅这个话题的订阅者在这次话题发布都不会响应
                    pSubscriber_next = pSubscriber->next;
                    pSubscriber->callback_func(pSubscriber);

                    pSubscriber = pSubscriber_next;
                }
            }

            pTopic = pTopic->next;
        }

        // 处理闹钟
        pAlarm = ltx_sys_alarm_list.next;
        pAlarm2 = &ltx_sys_alarm_list;
        while(pAlarm != NULL){
            if(pAlarm->flag){
                // 必须先移除闹钟再调用回调，不然无法在回调中创建调用自己的闹钟
                // ltx_Alarm_remove(pAlarm);
                // 不调用 remove，省得又遍历一遍，浪费时间
                pAlarm2->next = pAlarm->next;
                pAlarm->next = NULL;
                pAlarm->flag = 0;

                pAlarm->callback_alarm(pAlarm);

                // 反正 next 是 null 了，不进到下一轮 while 判断了，赶紧重新遍历吧
                break;
            }

            pAlarm2 = pAlarm; // 但是每次循环要多跑一行代码🤔。感觉还是有点亏，毕竟大部分情况是在轮询而不是移除，想不到更优雅的办法
            pAlarm = pAlarm->next;
        }
    }
}

void ltx_Sys_schedule_start(void){
    flag_schedule = 1;
}

void ltx_Sys_schedule_stop(void){
    flag_schedule = 0;
}
