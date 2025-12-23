    #include "ltx_log.h"

int ltx_Log_init(void){
    SEGGER_RTT_Init();

    return 0;
}

int ltx_Log_deinit(void){
    return 0;
}
