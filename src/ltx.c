#include "ltx.h"

volatile TickType_t realTicks; // 系统时间，溢出处理感觉不太需要，反正 alarm 和 timer 内部有自己的计数器

// ========== 活跃组件列表 ==========
struct ltx_Topic_stu ltx_sys_topic_queue = {
    .next = NULL,
};
struct ltx_Topic_stu *ltx_sys_topic_queue_tail = &ltx_sys_topic_queue;

struct ltx_Timer_stu ltx_sys_timer_list = {
    .prev = NULL,
    .next = NULL,
};
struct ltx_Timer_stu *ltx_sys_timer_list_tail = &ltx_sys_timer_list;

struct ltx_Alarm_stu ltx_sys_alarm_list = {
    .prev = NULL,
    .next = NULL,
};
struct ltx_Alarm_stu *ltx_sys_alarm_list_tail = &ltx_sys_alarm_list;

// ========== 定时器相关 ==========
void ltx_Timer_add(struct ltx_Timer_stu *timer){

    _LTX_IRQ_DISABLE();

    if(timer->next != NULL || ltx_sys_timer_list_tail == timer){ // 已经存在，不重复加入
        _LTX_IRQ_ENABLE();
        return ;
    }

    timer->prev = ltx_sys_timer_list_tail;
    ltx_sys_timer_list_tail->next = timer;
    ltx_sys_timer_list_tail = timer;

    _LTX_IRQ_ENABLE();
}

void ltx_Timer_remove(struct ltx_Timer_stu *timer){

    _LTX_IRQ_DISABLE();

    if(timer->next == NULL && timer->prev == NULL){ // 已经不在活跃列表中
        _LTX_IRQ_ENABLE();
        return ;
    }

    timer->prev->next = timer->next;
    if(ltx_sys_timer_list_tail == timer){ // 需要移除的这个节点是尾节点
        ltx_sys_timer_list_tail = timer->prev;
    }else {
        timer->next->prev = timer->prev;
        timer->next = NULL;
    }
    timer->prev = NULL;

    // 清除就绪标志位
    timer->topic.flag_is_pending = 0; // 就绪标志位清零

    _LTX_IRQ_ENABLE();
}

// ========== 闹钟相关 ==========
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down){
    
    _LTX_IRQ_DISABLE();
    alarm->tick_count_down = tick_count_down;
    if(alarm->next != NULL || ltx_sys_alarm_list_tail == alarm){ // 已经存在，不重复加入
        _LTX_IRQ_ENABLE();
        return ;
    }

    alarm->prev = ltx_sys_alarm_list_tail;
    ltx_sys_alarm_list_tail->next = alarm;
    ltx_sys_alarm_list_tail = alarm;

    _LTX_IRQ_ENABLE();
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    _LTX_IRQ_DISABLE();

    if(alarm->next == NULL && alarm->prev == NULL){ // 已经不在活跃列表中
        _LTX_IRQ_ENABLE();
        return ;
    }

    alarm->prev->next = alarm->next;
    if(ltx_sys_alarm_list_tail == alarm){ // 需要移除的这个节点是尾节点
        ltx_sys_alarm_list_tail = alarm->prev;
    }else {
        alarm->next->prev = alarm->prev;
        alarm->next = NULL;
    }
    alarm->prev = NULL;

    // 移除可能已经就绪的 topic
    // if(alarm->topic.next != NULL || (ltx_sys_topic_queue_tail == &(alarm->topic))){
    //     // 因为不是双向链表，所以要 O(n)
    // }
    // 不用遍历了，直接添加标志位清除
    alarm->topic.flag_is_pending = 0; // 就绪标志位清零

    _LTX_IRQ_ENABLE();
}

// ========== 话题相关 ==========
void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){

    _LTX_IRQ_DISABLE();
    if(subscriber->next != NULL || topic->subscriber_tail == subscriber){ // 已经存在，不重复添加
        // 但是不添加额外的成员变量的话，只能遍历所有话题才能避免一个订阅者订阅多个话题，先不管
        _LTX_IRQ_ENABLE();
        return ;
    }

    subscriber->prev = topic->subscriber_tail;
    topic->subscriber_tail->next = subscriber;
    topic->subscriber_tail = subscriber;

    _LTX_IRQ_ENABLE();
}

void ltx_Topic_unsubscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){

    _LTX_IRQ_DISABLE();

    if(subscriber->next == NULL && subscriber->prev == NULL){ // 已经不在活跃列表中
        _LTX_IRQ_ENABLE();
        return ;
    }

    subscriber->prev->next = subscriber->next;
    if(topic->subscriber_tail == subscriber){ // 需要移除的这个节点是尾节点
        topic->subscriber_tail = subscriber->prev;
    }else {
        subscriber->next->prev = subscriber->prev;
        subscriber->next = NULL;
    }
    subscriber->prev = NULL;

    _LTX_IRQ_ENABLE();
}

// 发布话题
void ltx_Topic_publish(struct ltx_Topic_stu *topic){
    
    _LTX_IRQ_DISABLE();

    topic->flag_is_pending = 1;
    if(topic->next != NULL || ltx_sys_topic_queue_tail == topic){ // 已经存在
        _LTX_IRQ_ENABLE();
        return ;
    }

    ltx_sys_topic_queue_tail->next = topic;
    ltx_sys_topic_queue_tail = topic;

    _LTX_SET_SCHEDULE_FLAG();
    _LTX_IRQ_ENABLE();
}

// ========== 系统调度相关 ==========
// 系统嘀嗒，由 systick/硬件定时器 每 tick 调用一次
void ltx_Sys_tick_tack(void){
    realTicks ++;

    struct ltx_Alarm_stu *pAlarm = ltx_sys_alarm_list.next;
    struct ltx_Alarm_stu *pAlarm_next; // 在移除闹钟时暂存它的 next 指针
    struct ltx_Timer_stu *pTimer = ltx_sys_timer_list.next;

    _LTX_IRQ_DISABLE();
    while(pAlarm != NULL){
        if(0 == --pAlarm->tick_count_down){
            // ltx_Topic_publish(&(pAlarm->topic));
            pAlarm->topic.flag_is_pending = 1;
            if(!(pAlarm->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm->topic))){ // 不存在于话题队列，推入
                ltx_sys_topic_queue_tail->next = &(pAlarm->topic);
                ltx_sys_topic_queue_tail = &(pAlarm->topic);
                _LTX_SET_SCHEDULE_FLAG();
            }
            // 移除这个闹钟
            if(ltx_sys_alarm_list_tail == pAlarm){ // 这个闹钟为尾节点
                ltx_sys_alarm_list_tail = pAlarm->prev;
                ltx_sys_alarm_list_tail->next = NULL;
                pAlarm->prev = NULL;
                break;
            }else {
                pAlarm->prev->next = pAlarm->next;
                pAlarm->next->prev = pAlarm->prev;
                pAlarm_next = pAlarm->next;
                pAlarm->prev = NULL;
                pAlarm->next = NULL;
                pAlarm = pAlarm_next;
            }
        }else {
            pAlarm = pAlarm->next;
        }
    }

    // 短暂出让？
    // _LTX_IRQ_ENABLE();
    // _LTX_IRQ_DISABLE();

    while(pTimer != NULL){
        if(0 == -- pTimer->tick_counts){
            pTimer->tick_counts = pTimer->tick_reload;
            // ltx_Topic_publish(&(pTimer->topic));
            pTimer->topic.flag_is_pending = 1;
            if(!(pTimer->topic.next != NULL || ltx_sys_topic_queue_tail == &(pTimer->topic))){ // 不存在于话题队列，推入
                ltx_sys_topic_queue_tail->next = &(pTimer->topic);
                ltx_sys_topic_queue_tail = &(pTimer->topic);
                _LTX_SET_SCHEDULE_FLAG();
            }
        }
        pTimer = pTimer->next;
    }
    _LTX_IRQ_ENABLE();
}

// 获取当前 tick 计数
TickType_t ltx_Sys_get_tick(void){
    return realTicks;
}

// 调度器，一般由主循环执行，也可以放在 pendsv 之类的最低优先级的软中断里
void ltx_Sys_scheduler(void){
    struct ltx_Topic_stu *pTopic;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;

    do{
        _LTX_CLEAR_SCHEDULE_FLAG(); // 清除调度标志位，为空闲休眠所设计
        // 不断弹出话题队列第一个节点
        if(ltx_sys_topic_queue.next != NULL){
            _LTX_IRQ_DISABLE();

            if(ltx_sys_topic_queue_tail == ltx_sys_topic_queue.next){ // 只有一个节点，移动尾指针
                ltx_sys_topic_queue_tail = &ltx_sys_topic_queue;
            }else { // 队列里还有其他节点要处理，不退出循环，避免函数出入开销，提高效率
                _LTX_SET_SCHEDULE_FLAG();
            }
            pTopic = ltx_sys_topic_queue.next; // 暂存指针
            ltx_sys_topic_queue.next = pTopic->next; // 弹出
            pTopic->next = NULL;

            _LTX_IRQ_ENABLE();

            if(!pTopic->flag_is_pending){ // 话题被取消
                continue;
            }
            pTopic->flag_is_pending = 0; // 就绪标志位清零
            
            pSubscriber = pTopic->subscriber_head.next;
            while(pSubscriber != NULL){
                // 加一行 next 暂存，不然如果回调里把自己取消订阅了，那么 next 就是 NULL，那链表后续所有订阅这个话题的订阅者在这次话题发布都不会响应
                pSubscriber_next = pSubscriber->next;
                pSubscriber->callback_func(pSubscriber);

                pSubscriber = pSubscriber_next;
            }
        }
    }while(_LTX_GET_SCHEDULE_FLAG);
}

// 空闲任务，如果 ltx_Sys_scheduler 放在软中断里，那么这个才能使用并且放在 main 函数最后
void ltx_Sys_idle_task(void){
    while(1){
        // 可以进入休眠，等待中断唤醒，或者执行一些 tickless 操作
    }
}
