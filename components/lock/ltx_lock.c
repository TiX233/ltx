#include "ltx_lock.h"

// 锁通用超时回调
void _ltx_Lock_alarm_cb(void *param){
    struct ltx_Lock_stu *pLock = container_of(param, struct ltx_Lock_stu, subscriber_alarm);

    // 调用用户定义锁超时回调
    pLock->timeout_callback(pLock);
}

/**
 * @brief   锁初始化
 * @param   lock: 锁对象指针
 * @param   timeout_callback: 锁超时回调，用户可以在回调中强制释放资源并调用 ltx_Lock_release 解锁
 * @retval  无
 */
void ltx_Lock_init(struct ltx_Lock_stu *lock, void (*timeout_callback)(struct ltx_Lock_stu *)){
    lock->flag_is_locked = 0;

    lock->topic_lock_release.flag_is_pending = 0;
    lock->topic_lock_release.subscriber_head.prev = NULL;
    lock->topic_lock_release.subscriber_head.next = NULL;
    lock->topic_lock_release.subscriber_tail = &(lock->topic_lock_release.subscriber_head);
    lock->topic_lock_release.next = NULL;

    // lock->alarm_time_out.tick_count_down = 0;
    lock->alarm_time_out.topic.flag_is_pending = 0;
    lock->alarm_time_out.topic.subscriber_head.prev = NULL;
    lock->alarm_time_out.topic.subscriber_head.next = &(lock->subscriber_alarm);
    lock->alarm_time_out.topic.subscriber_tail = &(lock->subscriber_alarm);
    lock->alarm_time_out.prev = NULL;
    lock->alarm_time_out.next = NULL;

    lock->subscriber_alarm.callback_func = _ltx_Lock_alarm_cb;
    lock->subscriber_alarm.prev = &(lock->alarm_time_out.topic.subscriber_head);
    lock->subscriber_alarm.next = NULL;

    lock->timeout_callback = timeout_callback;
}

/**
 * @brief   锁上锁并设置超时时间
 * @param   lock: 锁对象指针
 * @param   timeout: 锁超时时间，设置为 0 则代表持续等待直到 TickType_t 溢出
 * @retval  无
 */
void ltx_Lock_locked(struct ltx_Lock_stu *lock, TickType_t timeout){
    // 是否要考虑先判断锁是否已被上锁并返回值？这样更原子操作一点🤔
    // 不过作为裸机，一般占有锁也只会是产生在非中断里，在中断里一般只产生释放锁，所以也许不会被其他地方抢占后上锁。再说吧
    ltx_Alarm_add(&lock->alarm_time_out, timeout);

    lock->flag_is_locked = 1;
}

/**
 * @brief   锁手动释放
 * @param   lock: 锁对象指针
 * @retval  无
 */
void ltx_Lock_release(struct ltx_Lock_stu *lock){
    lock->flag_is_locked = 0;
    ltx_Alarm_remove(&lock->alarm_time_out);
    ltx_Topic_publish(&lock->topic_lock_release);
}

/**
 * @brief   订阅锁释放话题
 * @param   lock: 锁对象指针
 * @param   timeout: 锁超时时间
 * @retval  无
 */
void ltx_Lock_subscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber){
    ltx_Topic_subscribe(&lock->topic_lock_release, subscriber);
}

/**
 * @brief   取消订阅锁释放话题
 * @param   lock: 锁对象指针
 * @param   timeout: 锁超时时间
 * @retval  无
 */
void ltx_Lock_unsubscribe(struct ltx_Lock_stu *lock, struct ltx_Topic_subscriber_stu *subscriber){
    ltx_Topic_unsubscribe(&lock->topic_lock_release, subscriber);
}

/**
 * @brief   获取锁状态
 * @param   lock: 锁对象指针
 * @retval  返回 0 代表锁释放，返回 1 代表锁上锁
 */
uint8_t ltx_Lock_is_locked(struct ltx_Lock_stu *lock){
    return lock->flag_is_locked;
}
