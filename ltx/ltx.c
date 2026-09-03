#include "ltx.h"

volatile TickType_t realTicks; // 系统时间，溢出处理感觉不太需要，反正 alarm 内部有自己的计数器
volatile TickType_t intervalTicks = 1; // systick 调用间隔，不需要 tickless 的话这个固定为 1ms，开启 tickless 的话这个会动态变化

#if (ltx_cfg_CORE_NUM > 1)
// 多核心下全局自旋变量
volatile spin_num_t __g_spin_lock = 0;
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
        // 好像有点画蛇添足
        // _ltx_Sys_systick_set_reload(_ltx_Sys_systick_get_val());
        // _ltx_Sys_systick_clr_val(); // 触发重载，不需要吧，反正已经等于重载值了
    }
    _ltx_Sys_systick_resume(); // 恢复 systick
    
    intervalTicks -= tick_dec; // 理论上应该不会减成零...先不做检查
    realTicks += tick_dec;

    if(ltx_sys_alarm_list.next != NULL){ // 首个任务减去已经休眠的时间
        ltx_sys_alarm_list.next->diff_tick -= tick_dec;
    }else { // 组件链表没有任务
        alarm->diff_tick = tick_count_down;
        intervalTicks = (alarm->diff_tick > _SYSTICK_MAX_TICK) ? _SYSTICK_MAX_TICK : alarm->diff_tick;
        ltx_sys_alarm_list.next = alarm;
        alarm->prev = &ltx_sys_alarm_list;

    _ltx_Sys_systick_pause(); // 暂停 systick
        // 虽然会导致 systick 时间错位，但是应该影响不大，反正只有这个一个任务
        // 对于只有一个每 1ms 执行一次的任务的情况，那么 systick 实际周期会大于 1ms，但是既然每个毫秒都要调用，为什么还要开 tickless
        _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1); // 或许应该多减几顺便把这两条语句的时间补偿上去？
        _ltx_Sys_systick_clr_val(); // 触发重载

        _ltx_Sys_systick_clr_flag(); // 清除标志位，避免已经触发

    _ltx_Sys_systick_resume(); // 恢复 systick
    
        _LTX_CRITICAL_OUTO();
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
            _LTX_CRITICAL_OUTO();
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

    _ltx_Sys_systick_pause(); // 暂停 systick
#if 0
    // 不知道为什么用了这个补偿的话 systick 有概率直接触发
    _ltx_Sys_systick_set_reload((_ltx_Sys_systick_get_val()%_SYSTICK_COUNT_PER_TICK) + (intervalTicks-1)*_SYSTICK_COUNT_PER_TICK - 1);
    // _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - (_ltx_Sys_systick_get_val()%_SYSTICK_COUNT_PER_TICK)); // - 1);
#else
    // 不补偿，会导致 systick 被推迟一点
    _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1);
#endif
    _ltx_Sys_systick_clr_val(); // 触发重载
    _ltx_Sys_systick_clr_flag(); // 清除标志位，避免已经触发
    _ltx_Sys_systick_resume(); // 恢复 systick
    
#endif

    _LTX_CRITICAL_OUTO();
}

void ltx_Alarm_remove(struct ltx_Alarm_stu *alarm){
    
    // 移除，O(1)
    _LTX_CRITICAL_INTO();

    if(alarm->prev == NULL){ // 已经不在活跃列表中
        // 移除可能已经就绪的 topic
        alarm->topic.state &= (~0x01); // 就绪标志位清零
        _LTX_CRITICAL_OUTO();
        return ;
    }

    alarm->prev->next = alarm->next;
    if(alarm->next != NULL){
        alarm->next->prev = alarm->prev;
        alarm->next->diff_tick += alarm->diff_tick; // 应该不会溢出
        alarm->next = NULL;
    }
    alarm->prev = NULL;

    // 移除可能已经就绪的 topic
    alarm->topic.state &= (~0x01); // 就绪标志位清零

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
        topic->subscriber_head.next = subscriber;
    }

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
// 系统嘀嗒，由 systick/硬件定时器 中断服务函数调用
void ltx_Sys_tick_tack(void){
    realTicks += intervalTicks;

    // struct ltx_Alarm_stu *pAlarm = &(ltx_sys_alarm_list);
    struct ltx_Alarm_stu *pAlarm_next = ltx_sys_alarm_list.next; // 在移除闹钟时暂存它的 next 指针

    // O(1)
    _LTX_CRITICAL_INTO();

    // 如果前几个是相同时间，那么一起弹出，否则只弹出第一个
    if(pAlarm_next != NULL){
        if(pAlarm_next->diff_tick <= intervalTicks){ // 时间到，弹出
            // pAlarm_next->diff_tick = _ltx_Sys_systick_get_reload(); // 调试用
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
    _ltx_Sys_systick_pause(); // 暂停 systick

#if 0
    // 补偿
    uint32_t systick_val = _ltx_Sys_systick_get_reload() - _ltx_Sys_systick_get_val();
    _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1 - systick_val); // 或许应该多减几顺便把这两条语句的时间补偿上去？
#else
    _ltx_Sys_systick_set_reload(intervalTicks*_SYSTICK_COUNT_PER_TICK - 1);
#endif
    _ltx_Sys_systick_clr_val(); // 触发重载

    _ltx_Sys_systick_clr_flag(); // 清除标志位，避免已经触发
    _ltx_Sys_systick_resume(); // 恢复 systick

#endif
    
    _LTX_CRITICAL_OUTO();
}

// 获取当前 tick 计数
TickType_t ltx_Sys_get_tick(void){
#ifdef ltx_cfg_USE_TICKLESS
    TickType_t tick_get;
    _LTX_CRITICAL_INTO();
    // 感觉应该要判断一下 systick 是否已经触发过重载
    tick_get = realTicks + (_ltx_Sys_systick_get_reload() - _ltx_Sys_systick_get_val())/_SYSTICK_COUNT_PER_TICK;
    _LTX_CRITICAL_OUTO();
    return tick_get;
#else
    return realTicks;
#endif
}

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
                _LTX_CRITICAL_OUTO();

                // 用户回调运行在非临界区
                callback_retval |= pSubscriber->callback_func(pSubscriber);
                // 运行完回调后，调度器不应该触碰 topic、subscriber 的任何成员变量，因为内存可能已经在回调中被用户释放

                // 所以用户需要注意取消订阅的时机，如果越俎代庖帮别人(next)取消订阅并释放了内存，那下面这个就是野指针了，最好是它(next)自己回调里取消订阅
                // 最好是丢给 ctx 去管理，会安全一点，也就是使用 wait_topic 组件
                pSubscriber = pSubscriber_next;
                _LTX_CRITICAL_INTO();
            }

            // 回调执行完成后，占有标志位清零
            if(!callback_retval){ // 用户没有释放 topic 的内存才能操作这个标志位，否则会访问野指针
                pTopic_real_head->state &= (~0x02);
            }

            // 从头遍历
            pTopic_real_head = ltx_sys_topic_queue.next;
            pTopic_real_head_prev = &ltx_sys_topic_queue; // 设置为 pTopic_real_head 的前继
        }
        _LTX_CRITICAL_OUTO();

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
