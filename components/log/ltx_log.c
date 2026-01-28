#include "ltx_log.h"
#include "ltx_config.h"

ltx_weak int ltx_Log_init(void){
    SEGGER_RTT_Init();

    return 0;
}

ltx_weak int ltx_Log_deinit(void){
    return 0;
}
