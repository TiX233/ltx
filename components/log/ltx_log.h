/**
 * @file ltx_log.h
 * @author realTiX
 * @brief 调试信息输出用
 * @version 2.0
 * @date 2026-01-13 (2.0, 增加不同等级调试信息专用宏)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_LOG_H__
#define __LTX_LOG_H__

#include "SEGGER_RTT.h"

#define LOG_INFO                "# "
#define LOG_WARNNING            "W "
#define LOG_ERROR               "E "
#define LOG_DEBUG               "D "

#define LTX_LOG_STR(str)        SEGGER_RTT_WriteString(0, str)
#define LTX_LOG_FMT(fmt, ...)   SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)

// 不同等级的调试信息
#define LTX_LOG_DEBG(fmt, ...)  SEGGER_RTT_printf(0, LOG_DEBUG fmt, ##__VA_ARGS__)
#define LTX_LOG_INFO(fmt, ...)  SEGGER_RTT_printf(0, LOG_INFO fmt, ##__VA_ARGS__)
#define LTX_LOG_WARN(fmt, ...)  SEGGER_RTT_printf(0, LOG_WARNNING fmt, ##__VA_ARGS__)
#define LTX_LOG_ERRO(fmt, ...)  SEGGER_RTT_printf(0, LOG_ERROR fmt, ##__VA_ARGS__)

int ltx_Log_init(void);
int ltx_Log_deinit(void);

#endif // __LTX_LOG_H__
