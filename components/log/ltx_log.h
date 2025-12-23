#ifndef __LTX_LOG_H__
#define __LTX_LOG_H__

#include "SEGGER_RTT.h"

#define PRINT_LOG           "# "
#define PRINT_WARNNING      "W "
#define PRINT_ERROR         "E "
#define PRINT_DEBUG         "D "

#define LOG_STR(str)        SEGGER_RTT_WriteString(0, str)
#define LOG_FMT(fmt, ...)   SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)

int ltx_Log_init(void);
int ltx_Log_deinit(void);

#endif // __LTX_LOG_H__
