#include "ltx_lock.h"

void _ltx_Lock_alarm_cb(void *param){
    struct ltx_Lock_stu *pLock = container_of(param, struct ltx_Lock_stu, _alarm);

    // 调用超时回调
    if(pLock->lock){
        pLock->timeout_callback(pLock);
    }
}

#if 0
void _ltx_Lock_subscriber_cb(void *param){
    struct ltx_Lock_stu *pLock = container_of(param, struct ltx_Lock_stu, _subscriber);
    
    // 耗时且有死循环风险的操作，不放中断里
    ltx_Alarm_remove(&pLock->_alarm);

    ltx_Topic_publish(&pLock->topic_lock_release);
}
#endif

void ltx_Lock_init(struct ltx_Lock_stu *lock, void (*timeout_callback)(struct ltx_Lock_stu *)){
    lock->lock = 0;

    lock->topic_lock_release.flag = 0;
    lock->topic_lock_release.subscriber = NULL;
    lock->topic_lock_release.next = NULL;


    lock->_alarm.flag = 0;
    lock->_alarm.tick_count_down = 0;
    lock->_alarm.callback_alarm = _ltx_Lock_alarm_cb;
    lock->_alarm.next = NULL;

#if 0
    lock->_topic.flag = 0;
    lock->_topic.subscriber = NULL;
    lock->_topic.next = NULL;

    lock->_subscriber.callback_func = _ltx_Lock_subscriber_cb;
    lock->_subscriber.next = NULL;

    ltx_Topic_add(&lock->_topic);
    ltx_Topic_subscribe(&lock->_topic, &lock->_subscriber);
#endif

    ltx_Topic_add(&lock->topic_lock_release);

    lock->timeout_callback = timeout_callback;
}

void ltx_Lock_on(struct ltx_Lock_stu *lock, TickType_t timeout){
    ltx_Alarm_set_count(&lock->_alarm, timeout);
    ltx_Alarm_add(&lock->_alarm);

    lock->lock = 1;
}

void ltx_Lock_off(struct ltx_Lock_stu *lock){
    // if(lock->lock){
        lock->lock = 0;
#if 0
        ltx_Topic_publish(&lock->_topic);
#else
        // ltx_Alarm_remove(&lock->_alarm); 等闹钟超时自动 remove
        ltx_Topic_publish(&lock->topic_lock_release);
#endif
    // }
}

void ltx_Lock_subscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber){
    ltx_Topic_subscribe(&lock->topic_lock_release, subscriber);
}

void ltx_Lock_unsubscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber){
    ltx_Topic_unsubscribe(&lock->topic_lock_release, subscriber);
}

uint8_t ltx_Lock_is_on(struct ltx_Lock_stu *lock){
    return lock->lock;
}
