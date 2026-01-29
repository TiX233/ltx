#include "ltx.h"

volatile TickType_t realTicks; // 系统时间，溢出处理感觉不太需要，反正 alarm 和 timer 内部有自己的计数器
volatile TickType_t intervalTicks = 1; // systick 调用间隔，不需要 tickless 的话这个固定为 1ms，开启 tickless 的话这个会动态变化

// ========== 活跃组件列表 ==========
struct ltx_Topic_stu ltx_sys_topic_queue = {
    .next = NULL,
};
struct ltx_Topic_stu *ltx_sys_topic_queue_tail = &ltx_sys_topic_queue;

// 闹钟链表，顺序存储，每个节点存储自身与上个节点的执行间隔时间
struct ltx_Alarm_stu ltx_sys_alarm_list = {
    .diff_tick = 0,
    .prev = NULL,
    .next = NULL,
};

#if 0
struct ltx_Timer_stu ltx_sys_timer_list = {
    .prev = NULL,
    .next = NULL,
};
struct ltx_Timer_stu *ltx_sys_timer_list_tail = &ltx_sys_timer_list;

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

#endif

// ========== 闹钟相关 ==========
// 如果需要闹钟立即响应，那应该直接 publish 闹钟内部的 topic，而不是给 tick_count_down 传入 0，如果传入 0 则表示以最大值进行倒计时
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down){
    struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    TickType_t tick_add = 0;

    // 为 0 则以最大值倒计时
    tick_count_down = (tick_count_down == 0) ? -1 : tick_count_down;

    _LTX_IRQ_DISABLE();

    // 已经存在，移除重新倒计时，O(1)
    if(alarm->prev != NULL){
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick;
            alarm->next = NULL;
        }else { // 为最后一个节点，不移除，直接加倒计时
            // 算了，不好搞，要处理溢出
        }
        alarm->prev = NULL;
    }

// 计算已经休眠的时间
#ifdef ltx_cfg_USE_TICKLESS
    TickType_t tick_dec; 
    _ltx_Sys_systick_pause(); // 暂停 systick
    if(_ltx_Sys_systick_get_flag()){ // 如果 systick 已经触发了但还没进 systick 中断
        tick_dec = intervalTicks - 1;
    }else {
        tick_dec = (_ltx_Sys_systick_get_reload() - _ltx_Sys_systick_get_val())/_SYSTICK_COUNT_PER_TICK;
        // 理论上应该不会把为 0 的计数值赋值给重载值，因为会由于中断标志位触发而进上一个分支
        // 或许要减几个来补偿一下，但是要判断，不然会小于零，算了，真要搞高精度定时用啥 systick，更别说开了 tickless 唤醒也慢
        _ltx_Sys_systick_set_reload(_ltx_Sys_systick_get_val());
        // _ltx_Sys_systick_clr_val(); // 触发重载，不需要吧，反正已经等于重载值了
    }
    _ltx_Sys_systick_resume(); // 恢复 systick
    
    intervalTicks -= tick_dec; // 理论上应该不会减成零...先不做检查
    realTicks += tick_dec;

    if(ltx_sys_alarm_list.next != NULL){ // 首个任务减去已经休眠的时间
        ltx_sys_alarm_list.next->diff_tick = intervalTicks;
    }else { // 组件链表没有任务
        intervalTicks = (alarm->diff_tick > _SYSTICK_MAX_TICK) ? _SYSTICK_MAX_TICK : alarm->diff_tick;
        ltx_sys_alarm_list.next = alarm;
        alarm->prev = &ltx_sys_alarm_list;

        // 虽然会导致 systick 时间错位，但是应该影响不大，反正只有这个一个任务
        // 对于只有一个每 1ms 执行一次的任务的情况，那么 systick 实际周期会大于 1ms，但是既然每个毫秒都要调用，为什么还要开 tickless
        _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1); // 或许应该多减几顺便把这两条语句的时间补偿上去？
        _ltx_Sys_systick_clr_val(); // 触发重载

        _ltx_Sys_systick_clr_flag(); // 清除标志位，避免已经触发

        _LTX_IRQ_ENABLE();
        return ;
    }
#endif

    // 插入节点，O(N)，总比 V2 的 systick 每个 tick 一次 O(N) 要好一点
    // 不过对于频繁的任务，比如几毫秒一次，那其实每次只要遍历头部几个就能插入了，也接近 O(1)，
    // 对于不频繁的任务，可能几百毫秒才一次 O(N)，所以 V3 事实上是比 V2 要高效的
    while(pAlarm->next != NULL){
        if((pAlarm->next->diff_tick + tick_add) > tick_count_down){
            alarm->diff_tick = tick_count_down - tick_add;
            pAlarm->next->diff_tick -= alarm->diff_tick;

            alarm->prev = pAlarm;
            alarm->next = pAlarm->next;
            pAlarm->next = alarm;
            alarm->next->prev = alarm;

#ifdef ltx_cfg_USE_TICKLESS
            goto TAG_set_tickless; // 省得写两次以后忘了改某一个
#else
            _LTX_IRQ_ENABLE();
            return ;
#endif
        }
        tick_add += pAlarm->next->diff_tick;

        pAlarm = pAlarm->next;
    }

    alarm->diff_tick = tick_count_down - tick_add;
    alarm->prev = pAlarm;
    pAlarm->next = alarm;

// 如果启用 tickless
#ifdef ltx_cfg_USE_TICKLESS
TAG_set_tickless:    
    intervalTicks = (ltx_sys_alarm_list.next->diff_tick > _SYSTICK_MAX_TICK) ? _SYSTICK_MAX_TICK : ltx_sys_alarm_list.next->diff_tick;
    // _ltx_Sys_systick_set_reload((_ltx_Sys_systick_get_reload()%_SYSTICK_COUNT_PER_TICK) + (intervalTicks-1)*_SYSTICK_COUNT_PER_TICK - 1);
    _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - (_ltx_Sys_systick_get_reload()%_SYSTICK_COUNT_PER_TICK)); // - 1);
    _ltx_Sys_systick_clr_val(); // 触发重载
    
#endif

    _LTX_IRQ_ENABLE();
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    // 移除，O(1)，不过其实不太常用
    _LTX_IRQ_DISABLE();

    if(alarm->prev == NULL){ // 已经不在活跃列表中
        _LTX_IRQ_ENABLE();
        return ;
    }

    alarm->prev->next = alarm->next;
    if(alarm->next != NULL){
        alarm->next->prev = alarm->prev;
        alarm->next->diff_tick += alarm->diff_tick;
        alarm->next = NULL;
    }
    alarm->prev = NULL;

    // 移除可能已经就绪的 topic
    alarm->topic.flag_is_pending = 0; // 就绪标志位清零

    _LTX_IRQ_ENABLE();
}

// ========== 话题相关 ==========
void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){

    _LTX_IRQ_DISABLE();
    // if(subscriber->next != NULL || topic->subscriber_tail == subscriber){ // 已经存在，不重复添加
    if(subscriber->prev != NULL){ // 已经存在，不重复添加
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

    // if(subscriber->prev == NULL && subscriber->next == NULL){ // 已经不在活跃列表中
    if(subscriber->prev == NULL){ // 已经不在活跃列表中
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
// 系统嘀嗒，由 systick/硬件定时器 中断服务函数调用
void ltx_Sys_tick_tack(void){
    realTicks += intervalTicks;

    // struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

    // O(1)
    _LTX_IRQ_DISABLE();

    // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
    if(pAlarm_next != NULL){
        if(pAlarm_next->diff_tick <= intervalTicks){ // 时间到，弹出
            pAlarm_next->topic.flag_is_pending = 1;
            if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){ // 不存在于话题队列，推入
                ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                _LTX_SET_SCHEDULE_FLAG();
            }
            // 移除这个闹钟
            ltx_sys_alarm_list.next = pAlarm_next->next;
            pAlarm_next->prev = NULL;
            if(ltx_sys_alarm_list.next != NULL){

                // pAlarm_next->next->prev = &ltx_sys_alarm_list;
                pAlarm_next->next = NULL;

                // 判断有没有需要同时弹出的闹钟
                pAlarm_next = ltx_sys_alarm_list.next;
                while(pAlarm_next != NULL){
                    pAlarm_next->prev = &ltx_sys_alarm_list;
                    if(pAlarm_next->diff_tick == 0){
                        // 弹出
                        pAlarm_next->topic.flag_is_pending = 1;
                        if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){ // 不存在于话题队列，推入
                            ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                            ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                            _LTX_SET_SCHEDULE_FLAG();
                        }
                        // 移除这个闹钟
                        ltx_sys_alarm_list.next = pAlarm_next->next;
                        pAlarm_next->next = NULL;
                        pAlarm_next->prev = NULL;

                        pAlarm_next = ltx_sys_alarm_list.next;
                    }else {
                        break;
                    }
                }
            }
        }else { // 时间还没到，减少首个元素的时间差
            pAlarm_next->diff_tick -= intervalTicks;
        }
    }

// 如果开启了 tickless 功能则计算下次唤醒的时间
#ifdef ltx_cfg_USE_TICKLESS

    pAlarm_next = ltx_sys_alarm_list.next;
    if(pAlarm_next != NULL){
        intervalTicks = (pAlarm_next->diff_tick > _SYSTICK_MAX_TICK) ? _SYSTICK_MAX_TICK : pAlarm_next->diff_tick;
    }else { // 没有活跃闹钟了，以 systick 最大时间休眠
        intervalTicks = _SYSTICK_MAX_TICK;
    }

    // 应该不需要判断是否已经重载过，毕竟这里是 systick 中断，应该不会拖太久才被调用
    // 其他中断要是能抢几个毫秒的中断不放手那只能说代码够烂
    // _ltx_Sys_systick_pause(); // 暂停 systick

    uint32_t systick_val = _ltx_Sys_systick_get_reload() - _ltx_Sys_systick_get_val(); // 补偿？
    _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1 - systick_val); // 或许应该多减几顺便把这两条语句的时间补偿上去？
    _ltx_Sys_systick_clr_val(); // 触发重载

    // _ltx_Sys_systick_resume(); // 恢复 systick

#endif
    
    _LTX_IRQ_ENABLE();
}

// 获取当前 tick 计数
TickType_t ltx_Sys_get_tick(void){
#ifdef ltx_cfg_USE_TICKLESS
    TickType_t tick_get;
    _LTX_IRQ_DISABLE();
    // 感觉应该要判断一下 systick 是否已经触发过重载
    tick_get = realTicks + (_ltx_Sys_systick_get_reload() - _ltx_Sys_systick_get_val())/_SYSTICK_COUNT_PER_TICK;
    _LTX_IRQ_ENABLE();
    return tick_get;
#else
    return realTicks;
#endif
}

// 调度器，一般由主循环执行，也可以放在 pendsv 之类的最低优先级的软中断里
void ltx_Sys_scheduler(void){
    struct ltx_Topic_stu *pTopic;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;

#ifdef ltx_cfg_USE_IDLE_TASK
    // 执行退出空闲休眠的钩子函数，估计不太好用，先这样吧
    ltx_Hook_idle_out();
#endif

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

#ifdef ltx_cfg_USE_IDLE_TASK
    // 执行进入空闲休眠的钩子函数
    ltx_Hook_idle_in();
#endif
}

// 空闲任务，如果 ltx_Sys_scheduler 放在软中断里，那么这个才能使用并且放在 main 函数最后
// 或者自己直接在主循环放个 while(1) 也是一样的
ltx_weak void ltx_Sys_idle_task(void){
    while(1){
        // 可以进入休眠，等待中断唤醒
    }
}

// 进入空闲休眠前的钩子函数，一般用于关闭一些系统功能
ltx_weak void ltx_Hook_idle_in(void){

}

// 退出空闲休眠后的钩子函数，一般用于恢复系统功能
ltx_weak void ltx_Hook_idle_out(void){

}
