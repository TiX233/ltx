#include "ltx_log.h"
#include "ltx_config.h"

// 使用 DMA 外设输出的话
#if(_LTX_LOG_CHOOSE == _LTX_LOG_USE_DMA)
    #include "ltx_mtbuf.h"                                                  // 添加多 buf 支持
    uint8_t _ltx_log_out_bufs_static[_LTX_LOG_BUF_NUM][_LTX_LOG_BUF_SIZE];  // 静态创建 buf 内存
    struct ltx_mtbuf_stu _ltx_log_out_bufs[_LTX_LOG_BUF_NUM];               // 所有 buf 的对象的数组
    struct ltx_mtbuf_manager_stu _ltx_log_out_buf_manager;                  // 所有 buf 的管理对象
    uint8_t _ltx_log_out_dma_busy_flag;                                     // dma 忙标志位
    struct ltx_mtbuf_stu *_log_out_buf_dma_sending;                         // 正在被发送的 buf 对象的指针
#endif

// log 输出初始化
ltx_weak int ltx_Log_init(void){

    #if(_LTX_LOG_CHOOSE == _LTX_LOG_USE_RTT)
        SEGGER_RTT_Init();
    #elif(_LTX_LOG_CHOOSE == _LTX_LOG_USE_DMA)
        // 将静态创建的 buf 与 buf 对象联系起来
        for(uint8_t i = 0; i < _LTX_LOG_BUF_NUM; i ++){
            if(ltx_mtbuf_buf_init(&_ltx_log_out_bufs[i], _LTX_LOG_BUF_SIZE, _ltx_log_out_bufs_static[i]) != 0){
                while(1);
            }
        }
        // 初始化多 buf 管理者
        if(ltx_mtbuf_manager_init(&_ltx_log_out_buf_manager, _LTX_LOG_BUF_NUM, _ltx_log_out_bufs) != 0){
            while(1);
        }
    #endif

    return 0;
}

// log 输出反初始化
ltx_weak int ltx_Log_deinit(void){
    return 0;
}

#if(_LTX_LOG_CHOOSE == _LTX_LOG_USE_DMA)
// DMA 外设发送完成服务函数
ltx_weak void ltx_Log_dma_send_over_handler(void){
    // 释放 buffer
    ltx_mtbuf_read_over(&_ltx_log_out_buf_manager, _log_out_buf_dma_sending);

    // 检查有没有需要发送的 buffer
    struct ltx_mtbuf_stu *buf_next_send = ltx_mtbuf_read_get(&_ltx_log_out_buf_manager);
    if(buf_next_send != NULL){
        _log_out_buf_dma_sending = buf_next_send;
        _LTX_LOG_OUT_DMA(buf_next_send->buf_ptr, buf_next_send->data_size);
    }else { // 没有要发送的数据
        _LTX_LOG_DMA_BUSY_FLAG_CLR();
    }
}
#endif
