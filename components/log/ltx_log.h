/**
 * @file ltx_log.h
 * @author realTiX
 * @brief 调试信息输出用
 * @version 3.1
 * @date 2026-01-13 (2.0, 增加不同等级调试信息专用宏)
 * 
 *       2026-01-28 (3.0, 将日志打印初始化反初始化改为弱函数)
 *       2026-03-16 (3.1, 增加多缓存 DMA 外设打印支持)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_LOG_H__
#define __LTX_LOG_H__

#include "ltx_log_config.h"

#if(_LTX_LOG_CHOOSE == _LTX_LOG_USE_RTT)
// 使用 rtt 输出
    #include "SEGGER_RTT.h"

    #define LTX_LOG_STR(str)        SEGGER_RTT_WriteString(0, str)
    #define LTX_LOG_FMT(fmt, ...)   SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)

    // 不同等级的调试信息
    #define LTX_LOG_DEBG(fmt, ...)  SEGGER_RTT_printf(0, LOG_DEBUG fmt, ##__VA_ARGS__)
    #define LTX_LOG_INFO(fmt, ...)  SEGGER_RTT_printf(0, LOG_INFO fmt, ##__VA_ARGS__)
    #define LTX_LOG_WARN(fmt, ...)  SEGGER_RTT_printf(0, LOG_WARNNING fmt, ##__VA_ARGS__)
    #define LTX_LOG_ERRO(fmt, ...)  SEGGER_RTT_printf(0, LOG_ERROR fmt, ##__VA_ARGS__)
#elif(_LTX_LOG_CHOOSE == _LTX_LOG_USE_DMA)
// 使用 DMA 外设输出
    #include "ltx_mtbuf.h"
    extern uint8_t _ltx_log_out_dma_busy_flag;
    extern struct ltx_mtbuf_stu *_log_out_buf_dma_sending;
    extern struct ltx_mtbuf_manager_stu _ltx_log_out_buf_manager;

    #define LTX_LOG_STR(str)            do{ /* 可能会导致偶尔输出延迟，不想分配额外的管理线程就只能这样了 */ \
                                            struct ltx_mtbuf_stu *buf_get = ltx_mtbuf_write_get(&_ltx_log_out_buf_manager);\
                                            if(buf_get != NULL){\
                                                _LTX_IRQ_DISABLE();\
                                                if(!_LTX_LOG_DMA_BUSY_FLAG_GET()){ /* DMA 没在发送那么就直接发起发送 */\
                                                    _LTX_LOG_DMA_BUSY_FLAG_SET();\
                                                    buf_get->status = LTX_MTBUF_READING;\
                                                    _LTX_IRQ_ENABLE();\
                                                    _log_out_buf_dma_sending = buf_get;\
                                                    _LTX_LOG_OUT_DMA(buf_get->buf_ptr, snprintf((char *)buf_get->buf_ptr, _LTX_LOG_BUF_SIZE, str));\
                                                }else{\
                                                    /* DMA 正忙，所以不能发起，但是如果 DMA 发送完成中断在 writeover 前产生，那就不会发起下次通信 */\
                                                    /* 也就会导致此次的输出可能会延迟到下一次有新输出的时候一起打印出去 */\
                                                    _LTX_IRQ_ENABLE();\
                                                    buf_get->data_size = snprintf((char *)buf_get->buf_ptr, _LTX_LOG_BUF_SIZE, str);\
                                                    ltx_mtbuf_write_over(&_ltx_log_out_buf_manager, buf_get);\
                                                }\
                                            } /* 没有空闲 buf 则丢弃此次输出*/\
                                        }while(0)

    #define LTX_LOG_FMT(fmt, ...)       do{ /* 可能会导致偶尔输出延迟，不想分配额外的管理线程就只能这样了 */ \
                                            struct ltx_mtbuf_stu *buf_get = ltx_mtbuf_write_get(&_ltx_log_out_buf_manager);\
                                            if(buf_get != NULL){\
                                                _LTX_IRQ_DISABLE();\
                                                if(!_LTX_LOG_DMA_BUSY_FLAG_GET()){ /* DMA 没在发送那么就直接发起发送 */\
                                                    _LTX_LOG_DMA_BUSY_FLAG_SET();\
                                                    buf_get->status = LTX_MTBUF_READING;\
                                                    _LTX_IRQ_ENABLE();\
                                                    _log_out_buf_dma_sending = buf_get;\
                                                    _LTX_LOG_OUT_DMA(buf_get->buf_ptr, snprintf((char *)buf_get->buf_ptr, _LTX_LOG_BUF_SIZE, fmt, ##__VA_ARGS__));\
                                                }else{\
                                                    /* DMA 正忙，所以不能发起，但是如果 DMA 发送完成中断在 writeover 前产生，那就不会发起下次通信 */\
                                                    /* 也就会导致此次的输出可能会延迟到下一次有新输出的时候一起打印出去 */\
                                                    _LTX_IRQ_ENABLE();\
                                                    buf_get->data_size = snprintf((char *)buf_get->buf_ptr, _LTX_LOG_BUF_SIZE, fmt, ##__VA_ARGS__);\
                                                    ltx_mtbuf_write_over(&_ltx_log_out_buf_manager, buf_get);\
                                                }\
                                            } /* 没有空闲 buf 则丢弃此次输出*/\
                                        }while(0)

    // 不同等级的调试信息
    #define LTX_LOG_DEBG(fmt, ...)      LTX_LOG_FMT(LOG_DEBUG fmt, ##__VA_ARGS__)
    #define LTX_LOG_INFO(fmt, ...)      LTX_LOG_FMT(LOG_INFO fmt, ##__VA_ARGS__)
    #define LTX_LOG_WARN(fmt, ...)      LTX_LOG_FMT(LOG_WARNNING fmt, ##__VA_ARGS__)
    #define LTX_LOG_ERRO(fmt, ...)      LTX_LOG_FMT(LOG_ERROR fmt, ##__VA_ARGS__)
#else
// 不输出
    #define LTX_LOG_STR(str)        
    #define LTX_LOG_FMT(fmt, ...)   

    // 不同等级的调试信息
    #define LTX_LOG_DEBG(fmt, ...)  
    #define LTX_LOG_INFO(fmt, ...)  
    #define LTX_LOG_WARN(fmt, ...)  
    #define LTX_LOG_ERRO(fmt, ...)  
#endif

int ltx_Log_init(void);
int ltx_Log_deinit(void);
void ltx_Log_dma_send_over_handler(void);

#endif // __LTX_LOG_H__
