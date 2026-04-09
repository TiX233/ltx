/**
 * @file ltx_mtbuf.h
 * @author realTiX
 * @brief 多消费者多生产者环境下的多缓存管理
 * @version 0.1
 * @date 2026-03-16 (0.1, 初步完成设计)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_MTBUF_H__
#define __LTX_MTBUF_H__

#include "ltx.h"

// buffer 状态枚举
typedef enum {
    LTX_MTBUF_IDLE,           // buffer 空闲，没有有效数据，可以被用来写入
    LTX_MTBUF_WRITTING,       // buffer 正在写入，不应该被读取
    LTX_MTBUF_WAITE_OVER,     // buffer 已经存入有效数据，等待被读取
    LTX_MTBUF_READING,        // buffer 正在被读取或使用，不应该修改其中的数据
} ltx_mtbuf_state_e;

// 单个 buffer 对象结构体
struct ltx_mtbuf_stu {
    uint32_t id;            // buffer id，只会被管理器用到，用户可以不管
    ltx_mtbuf_state_e status; // buffer 状态
    uint32_t buf_size;      // buffer 总容量，单位字节
    uint32_t data_size;     // buffer 内存储数据数量，单位字节
    uint8_t *buf_ptr;       // buffer 内存指针，用户自行分配具体内存块
};

// 多 buffer 管理
struct ltx_mtbuf_manager_stu {
    uint8_t buf_nums;       // 所管理的 buff 数量，最多 32 个
    uint32_t buf_all_bits;  // 所管理的 buff 的所有 bit map
    uint32_t buf_now_bits;  // 所管理的 buff 的当前 bit map
    struct ltx_mtbuf_stu *bufs; // buf 对象的数组
};

// 初始化单个 buf 对象，需要用户自行静态/动态创建一个连续内存块并将头指针传入
int ltx_mtbuf_buf_init(struct ltx_mtbuf_stu *buf, uint32_t buf_size, uint8_t *mem_ptr);
// 初始化多 buf 管理结构体，需要用户在外部静态/动态创建一个 buf 对象的数组并将头指针传入
int ltx_mtbuf_manager_init(struct ltx_mtbuf_manager_stu *buf_manager, uint8_t buf_num, struct ltx_mtbuf_stu *bufs);

// 获取一个可以用于写入数据的空闲 buf 对象的指针，返回 NULL 则表示无空闲 buf 可供写入
struct ltx_mtbuf_stu *ltx_mtbuf_write_get(struct ltx_mtbuf_manager_stu *buf_manager);
// 获取一个可以已经填入数据的 buf 对象的指针，返回 NULL 则表示无有效 buf 可供读取
struct ltx_mtbuf_stu *ltx_mtbuf_read_get(struct ltx_mtbuf_manager_stu *buf_manager);

// 写入完某个 buf 后将其置为可供读取状态
void ltx_mtbuf_write_over(struct ltx_mtbuf_manager_stu *buf_manager, struct ltx_mtbuf_stu *buf);
// 使用完某个 buf 后将其置为空闲状态，可供下次写入
void ltx_mtbuf_read_over(struct ltx_mtbuf_manager_stu *buf_manager, struct ltx_mtbuf_stu *buf);


#endif // __LTX_MTBUF_H__
