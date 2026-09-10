#include "ltx.h"

#if (ltx_cfg_CORE_NUM > 1) && defined(ltx_cfg_USE_CTX)
    #include "ctx.h"
#endif

#ifdef ltx_cfg_USE_SPIN_LOCK
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
    // V4 下闹钟链表头 diff_tick 将作为链表内所有闹钟的时间基准
    // 假如这里是 10，而第一个节点是 20，那么代表第一个节点会在系统时间戳为 30 时弹出，便于 tickless
    // 每次添加新闹钟或事件队列空闲的时候都会获取时间戳弹出所有需要弹出的闹钟，然后把这个 diff_tick 改为新的时间戳
    .diff_tick = 0,
    .prev = NULL,
    .next = NULL,
};


// ========== 闹钟相关 ==========
// 如果需要闹钟立即响应，那应该直接 publish 闹钟内部的 topic，而不是给 tick_count_down 传入 0，如果传入 0 则表示以最大值进行倒计时
void ltx_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down){
    struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    TickType_t tick_add = 0;
    // 在临界区外获取最新的时间戳，因为用户自定义的 ltx_Sys_get_tick 可能会进入临界区，避免多次进入死锁
    TickType_t now_sys_tick = ltx_Sys_get_tick();
    // 我说复数就是勾石，s 有什么用
    TickType_t interval_tick;

    TickType_t tick_recent = 0;
    uint8_t flag_need_set_schedule = 0; // 把设置调度信号放到临界区外，提高拓展性

    // 为 0 则以最大值倒计时
    if(tick_count_down == 0){
        tick_count_down = LTX_MAX_TICK;
    }else if(tick_count_down == LTX_INFINITE_TICK){ // 无限则不加入闹钟链表
        _LTX_CRITICAL_INTO();
        // 已经存在，移除
        if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
            // O(n)
            for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
                if(pTopic->next == &alarm->topic){
                    if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                        ltx_sys_topic_queue_tail = pTopic;
                    }
                    pTopic->next = alarm->topic.next;
                    alarm->topic.next = NULL;
                    break;
                }
            }
        }
        // O(1)
        if(alarm->prev != NULL){
            alarm->prev->next = alarm->next;
            if(alarm->next != NULL){
                alarm->next->prev = alarm->prev;
                alarm->next->diff_tick += alarm->diff_tick;
                alarm->next = NULL;
            }
            alarm->prev = NULL;
        }
        _LTX_CRITICAL_OUTO();
        return ;
    }

    _LTX_CRITICAL_INTO();

    // 已经存在，移除重新倒计时
    if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
        // O(n)
        for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
            if(pTopic->next == &alarm->topic){
                if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                    ltx_sys_topic_queue_tail = pTopic;
                }
                pTopic->next = alarm->topic.next;
                alarm->topic.next = NULL;
                break;
            }
        }
    }
    // O(1)
    if(alarm->prev != NULL){
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
            alarm->next = NULL;
        }
        alarm->prev = NULL;
    }

    // 获取此时最近的闹钟的倒计时，如果新闹钟比它更近，那么通知调度器重新计算休眠时间
    if(ltx_sys_alarm_list.next == NULL){
        flag_need_set_schedule = 1;
        // interval_tick = 0;
    }else {
        interval_tick = now_sys_tick - ltx_sys_alarm_list.diff_tick;
        if(interval_tick){ // 时间戳有变动，那么先弹出到时的闹钟
            struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

            while(pAlarm_next != NULL){
                if(pAlarm_next->diff_tick <= interval_tick){ // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
                    interval_tick -= pAlarm_next->diff_tick;
                    // 推入事件队列
                    if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){
                        ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                        ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                        flag_need_set_schedule = 1;
                    }
                    // 移除这个闹钟
                    ltx_sys_alarm_list.next = pAlarm_next->next;
                    pAlarm_next->prev = NULL;
                    pAlarm_next->next = NULL;

                    pAlarm_next = ltx_sys_alarm_list.next;
                }else { // 时间还没到，减少首个元素的时间差
                    pAlarm_next->diff_tick -= interval_tick;
                    pAlarm_next->prev = &ltx_sys_alarm_list;
                    break;
                }
            }
        }
        if(ltx_sys_alarm_list.next != NULL){
            tick_recent = ltx_sys_alarm_list.next->diff_tick;
        }
    }
    ltx_sys_alarm_list.diff_tick = now_sys_tick;

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

            goto label_alarm_add_over;
        }
        tick_add += pAlarm->next->diff_tick;

        pAlarm = pAlarm->next;
    }

    alarm->diff_tick = tick_count_down - tick_add;
    alarm->prev = pAlarm;
    pAlarm->next = alarm;

label_alarm_add_over:

    if(ltx_sys_alarm_list.next->diff_tick < tick_recent){
        flag_need_set_schedule = 1;
    }

    _LTX_CRITICAL_OUTO();
    
    if(flag_need_set_schedule){
        _LTX_SET_SCHEDULE_FLAG();
    }
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    // 移除，O(1)
    _LTX_CRITICAL_INTO();

    // 移除可能已经就绪的 topic
    // alarm->topic.state &= (~0x01); // 就绪标志位清零
    // 这里当时应该是考虑到从事件队列单链表移除 topic 需要 O(n)，所以额外搞了个标记位，移除的时候就只是清除标记位而不从链表删除 topic
    // 但是当时还没写出 ctx，所有组件都需要手动管理，那么一般都是静态创建的
    // 但是 ctx 可能会动态创建组件，假如订阅了某个事件，此时如果订阅事件和闹钟事件同时触发
    // 如果先弹出事件回调，那么它会取消已有闹钟，但是此时闹钟 topic 虽然取消，但是 topic 依然保留在队列中
    // 如果任务结束，内存被自动释放，那么闹钟 topic 的内存就可能被其它新占有者修改，非常危险
    // 哪怕静态创建也非常危险，因为组件使用完后如果重新使用会被重新初始化，那么依然会破坏 topic 内存
    // 因为订阅事件和闹钟事件同时触发概率比较低，所以这个问题没有暴露出来，但是必须要解决
    // O(n) 也无所谓，本身就是小概率事件，不会说每次移除都要 O(n)，只有撞上刚好弹出闹钟就 remove 的情况才会进 O(n)

    if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
        // O(n)
        for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
            if(pTopic->next == &alarm->topic){
                if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                    ltx_sys_topic_queue_tail = pTopic;
                }
                pTopic->next = alarm->topic.next;
                alarm->topic.next = NULL;
                break;
            }
        }
    }

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

#if (ltx_cfg_CORE_NUM > 1)
// 给 ctx 多核用的，因为部分操作需要全程保持在临界区内不能有空隙
uint8_t __ltx_MC_Alarm_add(struct ltx_Alarm_stu *alarm, TickType_t tick_count_down, TickType_t now_sys_tick){
    struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    TickType_t tick_add = 0;
    // 我说复数就是勾石，s 有什么用
    TickType_t interval_tick;

    TickType_t tick_recent = 0;
    uint8_t flag_need_set_schedule = 0; // 把设置调度信号放到临界区外，提高拓展性

    // 为 0 则以最大值倒计时
    if(tick_count_down == 0){
        tick_count_down = LTX_MAX_TICK;
    }else if(tick_count_down == LTX_INFINITE_TICK){ // 无限则不加入闹钟链表
        // 已经存在，移除
        if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
            // O(n)
            for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
                if(pTopic->next == &alarm->topic){
                    if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                        ltx_sys_topic_queue_tail = pTopic;
                    }
                    pTopic->next = alarm->topic.next;
                    alarm->topic.next = NULL;
                    break;
                }
            }
        }
        // O(1)
        if(alarm->prev != NULL){
            alarm->prev->next = alarm->next;
            if(alarm->next != NULL){
                alarm->next->prev = alarm->prev;
                alarm->next->diff_tick += alarm->diff_tick;
                alarm->next = NULL;
            }
            alarm->prev = NULL;
        }
        return 0;
    }

    // 已经存在，移除重新倒计时
    if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
        // O(n)
        for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
            if(pTopic->next == &alarm->topic){
                if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                    ltx_sys_topic_queue_tail = pTopic;
                }
                pTopic->next = alarm->topic.next;
                alarm->topic.next = NULL;
                break;
            }
        }
    }
    // O(1)
    if(alarm->prev != NULL){
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
            alarm->next = NULL;
        }
        alarm->prev = NULL;
    }

    // 获取此时最近的闹钟的倒计时，如果新闹钟比它更近，那么通知调度器重新计算休眠时间
    if(ltx_sys_alarm_list.next == NULL){
        flag_need_set_schedule = 1;
        // interval_tick = 0;
    }else {
        interval_tick = now_sys_tick - ltx_sys_alarm_list.diff_tick;
        if(interval_tick){ // 时间戳有变动，那么先弹出到时的闹钟
            struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

            // O(1)
            while(pAlarm_next != NULL){
                if(pAlarm_next->diff_tick <= interval_tick){ // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
                    interval_tick -= pAlarm_next->diff_tick;
                    // 推入事件队列
                    if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){
                        ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                        ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                        flag_need_set_schedule = 1;
                    }
                    // 移除这个闹钟
                    ltx_sys_alarm_list.next = pAlarm_next->next;
                    pAlarm_next->prev = NULL;
                    pAlarm_next->next = NULL;

                    pAlarm_next = ltx_sys_alarm_list.next;
                }else { // 时间还没到，减少首个元素的时间差
                    pAlarm_next->diff_tick -= interval_tick;
                    pAlarm_next->prev = &ltx_sys_alarm_list;
                    break;
                }
            }
        }
        if(ltx_sys_alarm_list.next != NULL){
            tick_recent = ltx_sys_alarm_list.next->diff_tick;
        }
    }
    ltx_sys_alarm_list.diff_tick = now_sys_tick;

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

            goto label_alarm_add_over;
        }
        tick_add += pAlarm->next->diff_tick;

        pAlarm = pAlarm->next;
    }

    alarm->diff_tick = tick_count_down - tick_add;
    alarm->prev = pAlarm;
    pAlarm->next = alarm;

label_alarm_add_over:

    if(ltx_sys_alarm_list.next->diff_tick < tick_recent){
        flag_need_set_schedule = 1;
    }
    
    return flag_need_set_schedule;
}

void __ltx_MC_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    // 移除，O(1)

    // 移除可能已经就绪的 topic
    // alarm->topic.state &= (~0x01); // 就绪标志位清零
    // 这里当时应该是考虑到从事件队列单链表移除 topic 需要 O(n)，所以额外搞了个标记位，移除的时候就只是清除标记位而不从链表删除 topic
    // 但是当时还没写出 ctx，所有组件都需要手动管理，那么一般都是静态创建的
    // 但是 ctx 可能会动态创建组件，假如订阅了某个事件，此时如果订阅事件和闹钟事件同时触发
    // 如果先弹出事件回调，那么它会取消已有闹钟，但是此时闹钟 topic 虽然取消，但是 topic 依然保留在队列中
    // 如果任务结束，内存被自动释放，那么闹钟 topic 的内存就可能被其它新占有者修改，非常危险
    // 哪怕静态创建也非常危险，因为组件使用完后如果重新使用会被重新初始化，那么依然会破坏 topic 内存
    // 因为订阅事件和闹钟事件同时触发概率比较低，所以这个问题没有暴露出来，但是必须要解决
    // O(n) 也无所谓，本身就是小概率事件，不会说每次移除都要 O(n)，只有撞上刚好弹出闹钟就 remove 的情况才会进 O(n)

    if((alarm->topic.next != NULL) || (&alarm->topic == ltx_sys_topic_queue_tail)){ // 小概率事件：刚弹出闹钟就被 remove
        // O(n)
        for(struct ltx_Topic_stu *pTopic = &ltx_sys_topic_queue; pTopic->next != NULL; pTopic = pTopic->next){
            if(pTopic->next == &alarm->topic){
                if(pTopic->next == ltx_sys_topic_queue_tail){ // 是尾节点
                    ltx_sys_topic_queue_tail = pTopic;
                }
                pTopic->next = alarm->topic.next;
                alarm->topic.next = NULL;
                break;
            }
        }
    }

    if(alarm->prev != NULL){ // 在活跃列表中
        alarm->prev->next = alarm->next;
        if(alarm->next != NULL){
            alarm->next->prev = alarm->prev;
            alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
            alarm->next = NULL;
        }
        alarm->prev = NULL;
    }
}
#endif

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
// 调度器，一般由主循环执行，多核心/多线程 可重入。可以每个核的主循环都跑一个，也可以放在 rtos 的某一个或多个线程内
#if (ltx_cfg_CORE_NUM > 1)
void ltx_Sys_scheduler(uint8_t core_id){ // 似乎甚至可以调度器里面跑调度器套娃，根据 core id 用不同的唤醒信号。好像不行，里面有死循环退不出

    // 真正可弹出的队列头。因为有些任务可能正在被其它核使用，避免回调被多核重入，所以绕过它弹出下一个可用的
    struct ltx_Topic_stu *pTopic_real_head;
    struct ltx_Topic_stu *pTopic_real_head_prev = &ltx_sys_topic_queue;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;
    uint8_t callback_retval;
    
    TickType_t now_tick;
    TickType_t interval_tick;
    #ifdef ltx_cfg_USE_IDLE_SLEEP
        TickType_t recent_tick;
        uint8_t flag_do_not_sleep = 0;
    #endif

    while(1){
        label_pop_head:
        // 不断弹出话题队列第一个节点
        _LTX_CRITICAL_INTO();

        pTopic_real_head = ltx_sys_topic_queue.next;
        // pTopic_real_head_prev = &ltx_sys_topic_queue;

        while(ltx_sys_topic_queue.next != NULL){

            if(pTopic_real_head->flag_is_occupyed){ // 正在被某个核处理回调，但是又被重复推入，绕过它弹出下个可用对象，避免回调重入
                if(pTopic_real_head->next == NULL){ // 如果后面没有了，那就尝试从头遍历
                        _LTX_CRITICAL_OUTO();
                        // 从头遍历
                        // pTopic_real_head = ltx_sys_topic_queue.next;
                        pTopic_real_head_prev = &ltx_sys_topic_queue;
                        // 如果事件队列中只有这一个任务，那么相当于只有另一个核处理完这颗核才能处理
                        // 但是这里是临界区，如果这颗核一直呆在临界区尝试弹出，那就一直响应不了中断，除非另一颗核心释放任务
                        // 而且另一颗核的任务执行过程中有可能申请进入临界区，或者执行完后一定会申请进入临界区，那么就会发生死锁
                        // 所以这里必须是先出临界区再进临界区才能从头遍历
                        goto label_pop_head;
                    }else {
                        // 跳过它继续遍历
                        pTopic_real_head_prev = pTopic_real_head;
                        pTopic_real_head = pTopic_real_head->next;
                        continue;
                    }
            }else { // 可以弹出
                pTopic_real_head->flag_is_occupyed = 1; // 占有
                if(ltx_sys_topic_queue_tail == pTopic_real_head){ // 位于最后一个节点，移动尾指针
                    ltx_sys_topic_queue_tail = pTopic_real_head_prev;
                }
                // 弹出 real_head
                pTopic_real_head_prev->next = pTopic_real_head->next;
                pTopic_real_head->next = NULL;
            }
            // 执行回调
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
                pTopic_real_head->flag_is_occupyed = 0;
            }

            // 从头遍历
            pTopic_real_head = ltx_sys_topic_queue.next;
            pTopic_real_head_prev = &ltx_sys_topic_queue; // 设置为 pTopic_real_head 的前继
        }

        // 此时链表中没有任务，并且在临界区内，所以可以计算可休眠时间，也就是最近一个闹钟的倒计时
        // 如果有需要弹出的闹钟，那么不休眠，继续执行
        if(ltx_sys_alarm_list.next != NULL){
            _LTX_CRITICAL_OUTO();

            // 确保在临界区外获取用户平台 tick，因为用户可能会重复进入临界区导致死锁
            now_tick = ltx_Sys_get_tick();

            _LTX_CRITICAL_INTO();
            interval_tick = now_tick - ltx_sys_alarm_list.diff_tick;
            if(interval_tick){ // 时间戳有变动，那么先弹出到时的闹钟
                struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

                while(pAlarm_next != NULL){
                    if(pAlarm_next->diff_tick <= interval_tick){ // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
                        interval_tick -= pAlarm_next->diff_tick;
                        // 推入事件队列
                        if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){
                            ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                            ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                            
                            #ifdef ltx_cfg_USE_IDLE_SLEEP
                                flag_do_not_sleep = 1;
                            #endif
                        }
                        // 移除这个闹钟
                        ltx_sys_alarm_list.next = pAlarm_next->next;
                        pAlarm_next->prev = NULL;
                        pAlarm_next->next = NULL;

                        pAlarm_next = ltx_sys_alarm_list.next;
                    }else { // 时间还没到，减少首个元素的时间差
                        pAlarm_next->diff_tick -= interval_tick;
                        pAlarm_next->prev = &ltx_sys_alarm_list;
                        break;
                    }
                }
                ltx_sys_alarm_list.diff_tick = now_tick;
            }
            #ifdef ltx_cfg_USE_IDLE_SLEEP
                if(ltx_sys_alarm_list.next != NULL){ // 计算可休眠时间
                    recent_tick = ltx_sys_alarm_list.next->diff_tick;
                    _LTX_CRITICAL_OUTO();
                }else {
                    _LTX_CRITICAL_OUTO();
                    recent_tick = LTX_INFINITE_TICK;
                }
            #else
                _LTX_CRITICAL_OUTO();
            #endif

        }else { // 没有闹钟需要弹出
            _LTX_CRITICAL_OUTO();
            #ifdef ltx_cfg_USE_IDLE_SLEEP
                recent_tick = LTX_INFINITE_TICK;
            #endif
        }
        // 出临界区后，这个值可以给用户去操作硬件定时器
        // 或者如果调度器跑在 rtos 的一个线程，那么可以设置为等待信号量的超时时间，并且配置发布话题的启动调度信号为发送信号量
        // 不需要担心出临界区后又有中断添加了一个新的更近的闹钟，导致睡眠超时。
        // 因为添加闹钟的 api 会在里面判断新闹钟是否更近，是那就立即发送一次调度信号，那么调度器就会立即唤醒再次计算最近的闹钟倒计时
        
        #ifdef ltx_cfg_USE_IDLE_SLEEP
            // 进入空闲钩子
            if(flag_do_not_sleep){
                flag_do_not_sleep = 0;
            }else {
                ltx_hook_idle_in(core_id, recent_tick);
            }
        #endif
    }
}
#else
// 单核版本
void ltx_Sys_scheduler(uint8_t core_id){
    struct ltx_Topic_stu *pTopic_real_head;
    struct ltx_Topic_subscriber_stu *pSubscriber;
    struct ltx_Topic_subscriber_stu *pSubscriber_next;

    TickType_t now_tick;
    TickType_t interval_tick;
    #ifdef ltx_cfg_USE_IDLE_SLEEP
        TickType_t recent_tick;
        uint8_t flag_do_not_sleep = 0;
    #endif

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

        // 此时链表中没有任务，并且在临界区内，所以可以计算可休眠时间，也就是最近一个闹钟的倒计时
        // 如果有需要弹出的闹钟，那么不休眠，继续执行
        if(ltx_sys_alarm_list.next != NULL){
            _LTX_CRITICAL_OUTO();

            // 确保在临界区外获取用户平台 tick，因为用户可能会重复进入临界区导致死锁
            now_tick = ltx_Sys_get_tick();

            _LTX_CRITICAL_INTO();
            interval_tick = now_tick - ltx_sys_alarm_list.diff_tick;
            if(interval_tick){ // 时间戳有变动，那么先弹出到时的闹钟
                struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

                while(pAlarm_next != NULL){
                    if(pAlarm_next->diff_tick <= interval_tick){ // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
                        interval_tick -= pAlarm_next->diff_tick;
                        // 推入事件队列
                        if(!(pAlarm_next->topic.next != NULL || ltx_sys_topic_queue_tail == &(pAlarm_next->topic))){
                            ltx_sys_topic_queue_tail->next = &(pAlarm_next->topic);
                            ltx_sys_topic_queue_tail = &(pAlarm_next->topic);
                            
                            #ifdef ltx_cfg_USE_IDLE_SLEEP
                                flag_do_not_sleep = 1;
                            #endif
                        }
                        // 移除这个闹钟
                        ltx_sys_alarm_list.next = pAlarm_next->next;
                        pAlarm_next->prev = NULL;
                        pAlarm_next->next = NULL;

                        pAlarm_next = ltx_sys_alarm_list.next;
                    }else { // 时间还没到，减少首个元素的时间差
                        pAlarm_next->diff_tick -= interval_tick;
                        pAlarm_next->prev = &ltx_sys_alarm_list;
                        break;
                    }
                }
                ltx_sys_alarm_list.diff_tick = now_tick;
            }
            #ifdef ltx_cfg_USE_IDLE_SLEEP
                if(ltx_sys_alarm_list.next != NULL){ // 计算可休眠时间
                    recent_tick = ltx_sys_alarm_list.next->diff_tick;
                    _LTX_CRITICAL_OUTO();
                }else {
                    _LTX_CRITICAL_OUTO();
                    recent_tick = LTX_INFINITE_TICK;
                }
            #else
                _LTX_CRITICAL_OUTO();
            #endif

        }else { // 没有闹钟需要弹出
            _LTX_CRITICAL_OUTO();
            #ifdef ltx_cfg_USE_IDLE_SLEEP
                recent_tick = LTX_INFINITE_TICK;
            #endif
        }
        // 出临界区后，这个值可以给用户去操作硬件定时器
        // 或者如果调度器跑在 rtos 的一个线程，那么可以设置为等待信号量的超时时间，并且配置发布话题的启动调度信号为发送信号量
        // 不需要担心出临界区后又有中断添加了一个新的更近的闹钟，导致睡眠超时。
        // 因为添加闹钟的 api 会在里面判断新闹钟是否更近，是那就立即发送一次调度信号，那么调度器就会立即唤醒再次计算最近的闹钟倒计时
        
        #ifdef ltx_cfg_USE_IDLE_SLEEP
            // 进入空闲钩子
            if(flag_do_not_sleep){
                flag_do_not_sleep = 0;
            }else {
                ltx_hook_idle_in(core_id, recent_tick);
            }
        #endif
    }
}
#endif
