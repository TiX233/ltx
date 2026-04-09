#include "ltx_mtbuf.h"


// 初始化单个 buf 对象，需要用户自行静态/动态创建一个连续内存块并将头指针传入
int ltx_mtbuf_buf_init(struct ltx_mtbuf_stu *buf, uint32_t buf_size, uint8_t *mem_ptr){
    if(buf == NULL || mem_ptr == NULL){
        return -1;
    }
    buf->buf_ptr = mem_ptr;
    buf->buf_size = buf_size;
    buf->status = LTX_MTBUF_IDLE;
	
	return 0;
}

// 初始化多 buf 管理结构体，需要用户在外部静态/动态创建一个 buf 对象的数组并将头指针传入
int ltx_mtbuf_manager_init(struct ltx_mtbuf_manager_stu *buf_manager, uint8_t buf_num, struct ltx_mtbuf_stu *bufs){
    if(buf_num > 32){
        return -1;
    }
    if(buf_manager == NULL || bufs == NULL){
        return -2;
    }

    buf_manager->buf_nums = buf_num;
    buf_manager->bufs = bufs;
    buf_manager->buf_now_bits = 0;
    for(uint8_t i = 0; i < buf_num; i ++){
        if(&bufs[i] == NULL){
            buf_manager->buf_all_bits = 0;
            return -3;
        }
        buf_manager->buf_all_bits |= ((uint32_t)1 << i);
        bufs[i].id = i;
    }

    return 0;
}


// 获取一个可以用于写入数据的空闲 buf 对象的指针，返回 NULL 则表示无空闲 buf 可供写入
struct ltx_mtbuf_stu *ltx_mtbuf_write_get(struct ltx_mtbuf_manager_stu *buf_manager){
    _LTX_IRQ_DISABLE();
    if(buf_manager->buf_now_bits < buf_manager->buf_all_bits){
        for(uint8_t i = 0; i < buf_manager->buf_nums; i ++){
            if(!(buf_manager->buf_now_bits & ((uint32_t)1 << i))){
                buf_manager->buf_now_bits |= ((uint32_t)1 << i);
                buf_manager->bufs[i].status = LTX_MTBUF_WRITTING;
                _LTX_IRQ_ENABLE();
                return &(buf_manager->bufs[i]);
            }
        }
        // 一般不会走到这里
        _LTX_IRQ_ENABLE();
        return NULL;
    }else { // 无空闲 buf
        _LTX_IRQ_ENABLE();
        return NULL;
    }
}

// 获取一个已经填入数据的 buf 对象的指针，返回 NULL 则表示无有效 buf 可供读取
struct ltx_mtbuf_stu *ltx_mtbuf_read_get(struct ltx_mtbuf_manager_stu *buf_manager){
    _LTX_IRQ_DISABLE();
    if(buf_manager->buf_now_bits > 0){
        for(uint8_t i = 0; i < buf_manager->buf_nums; i ++){
            if(buf_manager->buf_now_bits & ((uint32_t)1 << i)){
                if(buf_manager->bufs[i].status == LTX_MTBUF_WAITE_OVER){ // 找到已经写完等待读取的 buf
                    buf_manager->bufs[i].status = LTX_MTBUF_READING;
                    _LTX_IRQ_ENABLE();
                    return &(buf_manager->bufs[i]);
                }else {
                    continue;
                }
            }
        }
        // buff 忙于写入或者正在被其他消费者读取
        _LTX_IRQ_ENABLE();
        return NULL;
    }else { // 没有有数据的 buf
        _LTX_IRQ_ENABLE();
        return NULL;
    }
}


// 写入完某个 buf 后将其置为可供读取状态
void ltx_mtbuf_write_over(struct ltx_mtbuf_manager_stu *buf_manager, struct ltx_mtbuf_stu *buf){
    _LTX_IRQ_DISABLE();
    buf->status = LTX_MTBUF_WAITE_OVER;
    _LTX_IRQ_ENABLE();
}

// 使用完某个 buf 后将其置为空闲状态，可供下次写入
void ltx_mtbuf_read_over(struct ltx_mtbuf_manager_stu *buf_manager, struct ltx_mtbuf_stu *buf){
    _LTX_IRQ_DISABLE();
    buf->status = LTX_MTBUF_IDLE;
    buf_manager->buf_now_bits &= ~((uint32_t)1 << buf->id);
    _LTX_IRQ_ENABLE();
}

