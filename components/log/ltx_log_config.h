#ifndef __LTX_LOG_CONFIG_H__
#define __LTX_LOG_CONFIG_H__

// 日志打印输出方式
#define _LTX_LOG_USE_NONE                   0 // 不输出
#define _LTX_LOG_USE_RTT                    1 // 使用 rtt 输出
#define _LTX_LOG_USE_DMA                    2 // 使用支持 DMA 的外设输出，比如 DMA 串口

// 选择日志打印输出方式
#define _LTX_LOG_CHOOSE                     _LTX_LOG_USE_DMA

// 不同等级的打印信息的前缀
#define LOG_INFO                            "# "
#define LOG_WARNNING                        "W "
#define LOG_ERROR                           "E "
#define LOG_DEBUG                           "D "

#if(_LTX_LOG_CHOOSE == _LTX_LOG_USE_DMA)
// 使用 DMA 外设输出，根据平台修改相关内容
    #define _LTX_LOG_BUF_NUM                10  // 可供存储输出内容的 buf 数量
    #define _LTX_LOG_BUF_SIZE               128 // 单个 buf 所能存储的字节数
    // DMA 忙标志位操作
    #define _LTX_LOG_DMA_BUSY_FLAG_GET()    (_ltx_log_out_dma_busy_flag)
    #define _LTX_LOG_DMA_BUSY_FLAG_SET()    do{_ltx_log_out_dma_busy_flag = 1;}while(0)
    #define _LTX_LOG_DMA_BUSY_FLAG_CLR()    do{_ltx_log_out_dma_busy_flag = 0;}while(0)
    // 最终具体的 buf 输出到 DMA 的途径
    #define _LTX_LOG_OUT_DMA(_ptr, _size)   do{HAL_UART_Transmit_DMA(&huart2_handler, (uint8_t *)_ptr, _size);\
                                                __HAL_DMA_DISABLE_IT(&hdma1ch3_handler, DMA_IT_HT);}while(0)
#endif


#endif // __LTX_LOG_CONFIG_H__
