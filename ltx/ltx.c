#include "ltx.h"

#if (ltx_cfg_CORE_NUM > 1)
#ifdef ltx_cfg_USE_CTX
#include "ctx.h"
#endif
#endif

// SYSTICK_TYPE_INTERRUPT 模式下由调度器管理时间戳
#if (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_INTERRUPT)
    volatile TickType_t real_ticks; // 系统时间，溢出处理感觉不太需要，反正 alarm 内部有自己的计数器
#elif (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_TIMESTAMP)
    volatile TickType_t last_sleep_tick; // 上次休眠的时刻
#endif

#if (ltx_cfg_CORE_NUM > 1)
    // 多核心下全局自旋变量/自旋锁
    spin_type_t __g_spin_lock;
#endif

// ========== 活跃组件列表 ==========
// 事件链表队列 链表头
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


// ========== 闹钟相关 ==========
// 如果需要闹钟立即响应，那应该直接 publish 闹钟内部的 topic，而不是给 tick_count_down 传入 0，如果传入 0 则表示以最大值进行倒计时
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down){
    struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    TickType_t tick_add = 0;

    // 为 0 则以最大值倒计时
    tick_count_down = (tick_count_down == 0) ? -1 : tick_count_down;

    _LTX_CRITICAL_INTO();

    // 已经存在，移除重新倒计时，O(1)
    if(alarm->prev != NULL){
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
            alarm->next = NULL;
        }
        alarm->prev = NULL;
    }

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

            _LTX_CRITICAL_OUTO();
            return ;
        }
        tick_add += pAlarm->next->diff_tick;

        pAlarm = pAlarm->next;
    }

    alarm->diff_tick = tick_count_down - tick_add;
    alarm->prev = pAlarm;
    pAlarm->next = alarm;

    _LTX_CRITICAL_OUTO();
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    // 移除，O(1)
    _LTX_CRITICAL_INTO();

    // 移除可能已经就绪的 topic
    alarm->topic.state &= (~0x01); // 就绪标志位清零

    if(alarm->prev != NULL){ // 在活跃列表中
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
            alarm->next = NULL;
        }
        alarm->prev = NULL;
    }

    _LTX_CRITICAL_OUTO();
}

// ========== 话题相关 ==========
// V4 版本订阅新话题会自动取消订阅原本的话题，避免多次订阅
void ltx_Topic_subscribe(struct ltx_Topic_stu *topic, struct ltx_Topic_subscriber_stu *subscriber){

    _LTX_CRITICAL_INTO();
    
    if(subscriber->prev != NULL){ // 已经订阅了某个话题，先取消订阅
        subscriber->prev->next = subscriber->next;
        if(subscriber->next != NULL){
            subscriber->next->prev = subscriber->prev;
            // subscriber->next = NULL;
        }
        // subscriber->prev = NULL;
    }

    subscriber->next = topic->subscriber_head.next;
    subscriber->prev = &topic->subscriber_head;
    if(topic->subscriber_head.next != NULL){
        topic->subscriber_head.next->prev = subscriber;
    }
    topic->subscriber_head.next = subscriber;

    _LTX_CRITICAL_OUTO();
}

// V4 版本不再需要提供被订阅的话题的指针
void ltx_Topic_unsubscribe(struct ltx_Topic_subscriber_stu *subscriber){

    _LTX_CRITICAL_INTO();

    if(subscriber->prev != NULL){
        subscriber->prev->next = subscriber->next;
        
        if(subscriber->next != NULL){
            subscriber->next->prev = subscriber->prev;
            subscriber->next = NULL;
        }
        subscriber->prev = NULL;
    }
    // 没有订阅任何话题则不做任何操作

    _LTX_CRITICAL_OUTO();
}

// 发布话题，默认版本。将事件对象推入事件队列最末尾
void ltx_Topic_publish(struct ltx_Topic_stu *topic){
    
    _LTX_CRITICAL_INTO();

    // 就绪标志位置 1
    topic->state |= 0x01;
    // 已经存在，不推入事件队列
    if(topic->next != NULL || ltx_sys_topic_queue_tail == topic){
        _LTX_CRITICAL_OUTO();
        return ;
    }

    ltx_sys_topic_queue_tail->next = topic;
    ltx_sys_topic_queue_tail = topic;

    _LTX_CRITICAL_OUTO();

    _LTX_SET_SCHEDULE_FLAG();
}

// 发布话题，高优先级版本。将事件对象插队事件队列头，事件可以更快得到处理
void ltx_Topic_publish_high_priority(struct ltx_Topic_stu *topic){
    
    _LTX_CRITICAL_INTO();

    topic->state |= 0x01;
    if(topic->next != NULL || ltx_sys_topic_queue_tail == topic){ // 已经存在
        // 感觉不需要额外将事件挪到队头
        // 不可能说先低优先级发布一次再高优先级发布一次，重要事件肯定是固定使用高优先级版本，已经存在那么也只能是存在于队头
        _LTX_CRITICAL_OUTO();
        return ;
    }

    // 事件队列为空
    if(ltx_sys_topic_queue.next == NULL){
        ltx_sys_topic_queue_tail->next = topic;
        ltx_sys_topic_queue_tail = topic;

        _LTX_CRITICAL_OUTO();

        _LTX_SET_SCHEDULE_FLAG();
        return ;
    }

    // 不为空，插入头部
    topic->next = ltx_sys_topic_queue.next;
    ltx_sys_topic_queue.next = topic;

    _LTX_CRITICAL_OUTO();

    // 不为空可以省略启动调度信号
    // _LTX_SET_SCHEDULE_FLAG();
}

// ========== 系统调度相关 ==========
#if (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_INTERRUPT)
// 系统嘀嗒，由 systick/硬件定时器 中断服务函数调用
void ltx_Sys_tick_tack(void){

    real_ticks ++;

    // O(1)
    _LTX_CRITICAL_INTO();

    // struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

    // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
    if(pAlarm_next != NULL){
        if(pAlarm_next->diff_tick <= 1){ // 时间到，弹出
            // pAlarm_next->diff_tick = _ltx_Sys_systick_get_reload(); // 调试用
            pAlarm_next->topic.state |= 1;
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
                        pAlarm_next->topic.state |= 1;
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
            pAlarm_next->diff_tick -= 1;
        }
    }
    
    _LTX_CRITICAL_OUTO();
}
#endif

// 获取当前 tick 计数，如果用户选择 ltx_cfg_SYSTICK_TYPE 为 SYSTICK_TYPE_INTERRUPT，那么不需要自定义 ltx_Sys_get_tick
// 如果 ltx_cfg_SYSTICK_TYPE 为 SYSTICK_TYPE_TIMESTAMP，那么下面这个要由用户改为自己平台的获取时间戳的实现
#if (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_INTERRUPT)
ltx_weak 
TickType_t ltx_Sys_get_tick(void){

    return real_ticks;
}
#endif


// 调度器，一般由主循环执行，多核心/多线程 可重入。可以每个核的主循环都跑一个，也可以放在 rtos 的某一个或多个线程内
#if (ltx_cfg_CORE_NUM > 1)
void ltx_Sys_scheduler(uint8_t core_id){ // 似乎甚至可以调度器里面跑调度器套娃，根据 core id 用不同的唤醒信号。好像不行，里面有死循环退不出

    // 真正可弹出的队列头。因为有些任务可能正在被其它核使用，避免回调被多核重入，所以绕过它弹出下一个可用的
    struct ltx_Topic_stu *pTopic_real_head;
    struct ltx_Topic_stu *pTopic_real_head_prev = &ltx_sys_topic_queue;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;
    uint8_t callback_retval;

    while(1){
        // 不断弹出话题队列第一个节点
        _LTX_CRITICAL_INTO();

        pTopic_real_head = ltx_sys_topic_queue.next;
        // pTopic_real_head_prev = &ltx_sys_topic_queue;

        while(ltx_sys_topic_queue.next != NULL){

            switch(pTopic_real_head->state){
                case 0x00: // 被取消，弹出不执行回调
                case 0x02: // 正在被另一颗核心处理中，且被重复推入但又被取消，弹出不执行回调
                    if(ltx_sys_topic_queue_tail == pTopic_real_head){ // 位于最后一个节点，移动尾指针
                        ltx_sys_topic_queue_tail = pTopic_real_head_prev;
                    }
                    // 弹出 real_head
                    pTopic_real_head_prev->next = pTopic_real_head->next;

                    if(pTopic_real_head->next == NULL){ // 如果后面没有了，那就尝试从头遍历
                        // 从头遍历
                        pTopic_real_head = ltx_sys_topic_queue.next;
                        pTopic_real_head_prev = &ltx_sys_topic_queue;
                    }else {
                        pTopic_real_head->next = NULL;
                        pTopic_real_head = pTopic_real_head_prev->next;
                    }
                    // 继续遍历
                    continue;

                case 0x03: // 正在被另一颗核心处理中，且被重复推入，绕过它弹出下个可用事件，避免回调重入

                    if(pTopic_real_head->next == NULL){ // 如果后面没有了，那就尝试从头遍历
                        // 从头遍历
                        pTopic_real_head = ltx_sys_topic_queue.next;
                        pTopic_real_head_prev = &ltx_sys_topic_queue;
                    }else {
                        pTopic_real_head_prev = pTopic_real_head;
                        pTopic_real_head = pTopic_real_head->next;
                    }
                    // 继续遍历
                    continue;

                case 0x01: // 正常弹出执行回调
                    pTopic_real_head->state = 0x02; // 占有
                    if(ltx_sys_topic_queue_tail == pTopic_real_head){ // 位于最后一个节点，移动尾指针
                        ltx_sys_topic_queue_tail = pTopic_real_head_prev;
                    }
                    // 弹出 real_head
                    pTopic_real_head_prev->next = pTopic_real_head->next;
                    pTopic_real_head->next = NULL;

                    // 执行回调
                    // break;
            }
            
            callback_retval = 0;
            pSubscriber = pTopic_real_head->subscriber_head.next;
            while(pSubscriber != NULL){
                // 加一行 next 暂存，不然如果回调里把自己取消订阅了，那么 next 就是 NULL，那链表后续所有订阅这个话题的订阅者在这次话题发布都不会响应
                pSubscriber_next = pSubscriber->next;
                ltx_hook_before_user_call_back();
                _LTX_CRITICAL_OUTO();

                // 用户回调运行在非临界区
                callback_retval |= pSubscriber->callback_func(pSubscriber);
                // 运行完回调后，调度器不应该触碰 topic、subscriber 的任何成员变量，因为内存可能已经在回调中被用户释放

                // 所以用户需要注意取消订阅的时机，如果越俎代庖帮别人(next)取消订阅并释放了内存，那下面这个就是野指针了，最好是它(next)自己回调里取消订阅
                // 最好是丢给 ctx 去管理，会安全一点，也就是使用 wait_topic 组件
                pSubscriber = pSubscriber_next;
                _LTX_CRITICAL_INTO();
                ltx_hook_after_user_call_back();
            }

            // 回调执行完成后，占有标志位清零
            if(!callback_retval){ // 用户没有释放 topic 的内存才能操作这个标志位，否则会访问野指针
                pTopic_real_head->state &= (~0x02);
            }

            // 从头遍历
            pTopic_real_head = ltx_sys_topic_queue.next;
            pTopic_real_head_prev = &ltx_sys_topic_queue; // 设置为 pTopic_real_head 的前继
        }
        // 时间戳调度，在事件队列空闲时判断是否有闹钟需要弹出
        #if (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_TIMESTAMP)
            TickType_t tick_now = ltx_Sys_get_tick();


        #endif
        _LTX_CRITICAL_OUTO();

        #if (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_INTERRUPT)
            
        #elif (ltx_cfg_SYSTICK_TYPE == SYSTICK_TYPE_TIMESTAMP)
            
        #endif

        #ifdef ltx_cfg_USE_IDLE_HOOK
            // 进入空闲钩子
            ltx_Hook_idle_in(core_id);
        #endif
    }
}
#else
// 单核版本
void ltx_Sys_scheduler(uint8_t core_id){
    struct ltx_Topic_stu *pTopic_real_head;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;

    while(1){
        // 不断弹出话题队列第一个节点
        _LTX_CRITICAL_INTO();

        while(ltx_sys_topic_queue.next != NULL){
            pTopic_real_head = ltx_sys_topic_queue.next;

            if(ltx_sys_topic_queue_tail == ltx_sys_topic_queue.next){ // 位于最后一个节点，移动尾指针
                ltx_sys_topic_queue_tail = &ltx_sys_topic_queue;
            }
            // 弹出
            ltx_sys_topic_queue.next = pTopic_real_head->next;
            pTopic_real_head->next = NULL;

            if(pTopic_real_head->state){
                pTopic_real_head->state = 0;

                pSubscriber = pTopic_real_head->subscriber_head.next;
                while(pSubscriber != NULL){
                    // 加一行 next 暂存，不然如果回调里把自己取消订阅了，那么 next 就是 NULL，那链表后续所有订阅这个话题的订阅者在这次话题发布都不会响应
                    pSubscriber_next = pSubscriber->next;
                    _LTX_CRITICAL_OUTO();

                    // 用户回调运行在非临界区
                    pSubscriber->callback_func(pSubscriber);
                    // 运行完回调后，调度器不应该触碰 topic、subscriber 的任何成员变量，因为内存可能已经在回调中被用户释放

                    // 所以用户需要注意取消订阅的时机，如果越俎代庖帮别人(next)取消订阅并释放了内存，那下面这个就是野指针了，最好是它(next)自己回调里取消订阅
                    // 最好是丢给 ctx 去管理，会安全一点，也就是使用 wait_topic 组件
                    pSubscriber = pSubscriber_next;
                    _LTX_CRITICAL_INTO();
                }
            }
        }
        _LTX_CRITICAL_OUTO();

#ifdef ltx_cfg_USE_IDLE_HOOK
        // 进入空闲钩子
        ltx_Hook_idle_in(core_id);
#endif
    }
}
#endif


#ifdef ltx_cfg_USE_IDLE_HOOK
// 进入空闲的钩子函数，一般用于关闭一些系统功能或者休眠，没有特殊要求则保留一句 __WFE(); 或者什么都不干
ltx_weak void ltx_Hook_idle_in(uint8_t core_id){
    // 样例：

    // 判断是否长期空闲，决定是否关闭 xx 外设

    // cpu 休眠，等待事件唤醒
    // __WFE();
    // 或者如果调度器跑在 rtos 线程内的话，则等待信号量
    // 获取空闲时的tick = ltx_Sys_get_tick();
    // 是否信号量超时 = 获取信号量(&sem_ltx, 超时时间为最近的 alarm 触发的倒计时);
    // if(获取空闲时的tick + 最近的 alarm 触发的倒计时 <= ltx_Sys_get_tick()) 总之就是是否弹出闹钟链表头节点
}
#endif
