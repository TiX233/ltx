#include "ltx_cmd.h"
#include <stdio.h>
#include "ltx_param.h"
#include "ltx.h"
#include "ltx_app.h"
#include "myAPP_system.h"
#include "ltx_log.h"

typedef struct {
    const char *cmd_name;
    const char *brief;
    void (*cmd_cb)(uint8_t argc, char *argv[]);
} ltx_Cmd_item;

void cmd_cb_echo(uint8_t argc, char *argv[]);
void cmd_cb_help(uint8_t argc, char *argv[]);
void cmd_cb_hello(uint8_t argc, char *argv[]);
void cmd_cb_print(uint8_t argc, char *argv[]);
void cmd_cb_alarm(uint8_t argc, char *argv[]);
void cmd_cb_reboot(uint8_t argc, char *argv[]);
void cmd_cb_param(uint8_t argc, char *argv[]);
void cmd_cb_ltx_app(uint8_t argc, char *argv[]);

ltx_Cmd_item cmd_list[] = {
    {
        .cmd_name = "echo",
        .brief = "return 2nd param to test uart",
        .cmd_cb = cmd_cb_echo,
    },
    {
        .cmd_name = "hello",
        .brief = "hello",
        .cmd_cb = cmd_cb_hello,
    },
    {
        .cmd_name = "help",
        .brief = "print brief of commands",
        .cmd_cb = cmd_cb_help,
    },

    {
        .cmd_name = "print",
        .brief = "track a data item and print it whenever it is updated",
        .cmd_cb = cmd_cb_print,
    },

    {
        .cmd_name = "param",
        .brief = "read or write some param",
        .cmd_cb = cmd_cb_param,
    },

    {
        .cmd_name = "alarm",
        .brief = "set an alarm for test",
        .cmd_cb = cmd_cb_alarm,
    },

    {
        .cmd_name = "reboot",
        .brief = "reboot",
        .cmd_cb = cmd_cb_reboot,
    },

    {
        .cmd_name = "ltx_app",
        .brief = "manage ltx apps",
        .cmd_cb = cmd_cb_ltx_app,
    },


    // end of list:
    {
        .cmd_name = " ",
        .cmd_cb = NULL,
    },
};

// 返回 0 代表匹配成功
int my_str_cmp(const char *str1, char *str2){
    uint32_t i;
    for(i = 0; i < CMD_BUF_SIZE - 1; i++){
        if(str1[i] == '\0' || str1[i] == ' '){
            if(str2[i] == '\0' || str2[i] == ' ' || str2[i] == '\n'){
                str2[i] = '\0';
                return 0;
            }else {
                return -1;
            }
        }
        if(str1[i] != str2[i]){
            return -1;
        }
    }

    return 0;
}


void cmd_cb_echo(uint8_t argc, char *argv[]){
    if(argc > 1){
        if(my_str_cmp("-h", argv[1]) == 0){
            goto Useage_echo;
        }
        LOG_FMT(PRINT_LOG"%s\n", argv[1]);
    }else {
        LOG_FMT(PRINT_WARNNING"Need param to echo!\n");
    }
    return ;
Useage_echo:
    LOG_FMT(PRINT_LOG"Useage: This function will return the first param to test uart\n");
}

void cmd_cb_hello(uint8_t argc, char *argv[]){
    LOG_STR(PRINT_LOG"Ciallo World~ (∠・ω< )⌒☆\n");
}

void cmd_cb_help(uint8_t argc, char *argv[]){
    uint8_t i;
    
    if(argc > 1){
        for(i = 0; cmd_list[i].cmd_name[0] != ' '; i ++){
            if(my_str_cmp(cmd_list[i].cmd_name, argv[1]) == 0){
                LOG_FMT(PRINT_LOG"%s   -   %s\n", cmd_list[i].cmd_name, cmd_list[i].brief);
                return;
            }
        }
    }else {
        for(i = 0; cmd_list[i].cmd_name[0] != ' '; i ++){
            LOG_FMT(PRINT_LOG"%s:\n\t%s\n", cmd_list[i].cmd_name, cmd_list[i].brief);
        }

        return ;
    }

    LOG_FMT(PRINT_WARNNING"Unknown cmd: %s, Type /help to list all commands\n", argv[1]);
}

void alarm_cb_cmd_test_alarm(void *param){
    LOG_FMT(PRINT_LOG"Alarm ring: %d\n", ltx_Sys_get_tick());
}
struct ltx_Alarm_stu alarm_cmd_test = {
    .flag = 0,
    
    .tick_count_down = 0,
    .callback_alarm = alarm_cb_cmd_test_alarm,

    .next = NULL,
};

void cmd_cb_alarm(uint8_t argc, char *argv[]){
    if(argc != 2){
        goto Useage_alarm;
    }
    uint32_t ticks_alarm;
    sscanf(argv[1], "%d", &ticks_alarm);

    if(ticks_alarm < 1){
        alarm_cb_cmd_test_alarm(NULL);
        return ;
    }

    ltx_Alarm_set_count(&alarm_cmd_test, ticks_alarm);
    ltx_Alarm_add(&alarm_cmd_test);

    LOG_FMT(PRINT_LOG"Tick now: %d\n", ltx_Sys_get_tick());
    LOG_FMT(PRINT_LOG"Alarm will ring after %d ticks(%d)\n", ticks_alarm, ticks_alarm + ltx_Sys_get_tick());

    return ;
Useage_alarm:
    LOG_FMT(PRINT_LOG"Useage: %s <ticks>\n", argv[0]);
}

void cmd_cb_reboot(uint8_t argc, char *argv[]){
    NVIC_SystemReset();
}

void cmd_cb_ltx_app(uint8_t argc, char *argv[]){
    enum ltx_app_option_e{
        RA_list_app = 0,
        RA_list_task,
        RA_kill_app,
        RA_pause_app,
        RA_resume_app,
        RA_kill_task,
        RA_pause_task,
        RA_resume_task,
    };

    const char *options[] = {
        [RA_list_app] = "-list_app",
        [RA_list_task] = "-list_task",

        [RA_kill_app] = "-kill_app",
        [RA_pause_app] = "-pause_app",
        [RA_resume_app] = "-resume_app",

        [RA_kill_task] = "-kill_task",
        [RA_pause_task] = "-pause_task",
        [RA_resume_task] = "-resume_task",
        " ",
    };

    if(argc < 2){
        goto Useage_ltx_app;
    }

    uint8_t i = 0;
    for(; options[i][0] != ' '; i ++){
        if(my_str_cmp(options[i], argv[1]) == 0){
            break;
        }
    }

    struct ltx_App_stu **pApp = &(ltx_sys_app_list.next);
    struct ltx_Task_stu **pTask;
    switch(i){
        case RA_list_app:
            LOG_STR(PRINT_LOG"All apps:\n");
            while((*pApp) != NULL){
                LOG_FMT("\t%s: %s\n", (*pApp)->name, (*pApp)->status?"running":"pause");
                pApp = &((*pApp)->next);
            }
            break;
            
        case RA_list_task:
            if(argc < 3){
                LOG_STR(PRINT_WARNNING"Please provide app name!\n");
                goto Useage_ltx_app;
            }
            while((*pApp) != NULL){
                if(my_str_cmp((*pApp)->name, argv[2]) == 0){
                    
                    pTask = &((*pApp)->task_list);
                    LOG_STR(PRINT_LOG"All tasks:\n");
                    while((*pTask) != NULL){
                        LOG_FMT("\t%s: %s\n", (*pTask)->name, (*pTask)->status?"running":"pause");
                        pTask = &((*pTask)->next);
                    }
                    return ;
                }
                pApp = &((*pApp)->next);
            }
            LOG_FMT(PRINT_WARNNING"App(%s) not found!\n", argv[2]);
            
            break;
            
        case RA_kill_app:
        case RA_pause_app:
        case RA_resume_app:
            if(argc < 3){
                goto Useage_ltx_app;
            }
            while((*pApp) != NULL){
                if(my_str_cmp((*pApp)->name, argv[2]) == 0){
                    switch(i){
                        case RA_kill_app:
                            ltx_App_destroy((*pApp));

                            break;

                        case RA_pause_app:
                            ltx_App_pause((*pApp));

                            break;
                            
                        case RA_resume_app:
                            ltx_App_resume((*pApp));

                            break;
                    }
                    LOG_FMT(PRINT_LOG"App(%s) set okay\n", argv[2]);
                    return ;
                }
                pApp = &((*pApp)->next);
            }
            LOG_FMT(PRINT_WARNNING"App(%s) not found!\n", argv[2]);

            break;
            
        case RA_kill_task:
        case RA_pause_task:
        case RA_resume_task:
            if(argc < 4){
                goto Useage_ltx_app;
            }
            while((*pApp) != NULL){
                if(my_str_cmp((*pApp)->name, argv[2]) == 0){
                    
                    pTask = &((*pApp)->task_list);
                    while((*pTask) != NULL){
                        if(my_str_cmp((*pTask)->name, argv[3]) == 0){

                            switch(i){
                                case RA_kill_task:
                                    ltx_Task_destroy((*pTask));

                                    break;

                                case RA_pause_task:
                                    ltx_Task_pause((*pTask));
                                    
                                    break;

                                case RA_resume_task:
                                    ltx_Task_resume((*pTask));
                                    
                                    break;
                            }
                            LOG_FMT(PRINT_LOG"Task(%s) set okay\n", argv[2]);
                            return ;
                        }
                        pTask = &((*pTask)->next);
                    }
                    LOG_FMT(PRINT_WARNNING"Task(%s) not found!\n", argv[3]);
                    return ;
                }
                pApp = &((*pApp)->next);
            }
            LOG_FMT(PRINT_WARNNING"App(%s) not found!\n", argv[2]);

            break;

        default:
            LOG_FMT(PRINT_WARNNING"Unknown option:%s\n", argv[1]);
            goto ltx_app_print_all_option;
            break;
    }

    return ;

Useage_ltx_app:
    LOG_FMT(PRINT_LOG"Useage: %s <option> [app_name] [task_name]\n", argv[0]);
ltx_app_print_all_option:
    LOG_STR(PRINT_LOG"All options:\n");
    for(uint8_t j = 0; options[j][0] != ' '; j ++){
        LOG_FMT("\t%s\n", options[j]);
    }
}


// 数据更新追踪打印相关内容
// 心跳计数数据更新打印回调
void print_cb_heart_beat(void *param){
    extern uint32_t heart_beat_count;
    LOG_FMT("Heartbeat: %d\n", heart_beat_count);
}

// 可供打印的数据对象
struct {
    const char *item_name;
    struct ltx_Topic_stu *topic;
    struct ltx_Topic_subscriber_stu subscriber;
} print_data_item_list[] = {
    {
        .item_name = "heart_beat",
        .topic = &(task_heart_beat.topic),
        .subscriber = {
            .callback_func = print_cb_heart_beat,

            .next = NULL,
        },
    },


    // 列表结尾项
    {
        .item_name = " ",
    },
};

// 数据更新打印订阅设置命令
void cmd_cb_print(uint8_t argc, char *argv[]){
    if(argc < 2){
        goto Useage_print;
    }

enum print_option_e{
    PO_START = 0,
    PO_STOP = 1,
    PO_LIST = 2,
};

    const char *print_option_list[] = {
        [PO_START] = "-start",
        [PO_STOP] = "-stop",
        [PO_LIST] = "-list",

        " ",
    };

    uint8_t i;
    for(i = 0; print_option_list[i][0] != ' ' && my_str_cmp(print_option_list[i], argv[1]) != 0; i++);

    switch(i){
        case PO_START:
            if(argc != 3){
                LOG_STR(PRINT_WARNNING"Please enter the data name correctly!\n");

                goto Useage_print;
            }

            // LOG_STR(PRINT_DEBUG"going into PO_START\n");

            for(i = 0; print_data_item_list[i].item_name[0] != ' '; i ++){
                if(my_str_cmp(print_data_item_list[i].item_name, argv[2]) == 0){
                    LOG_FMT(PRINT_LOG"Start track data \"%s\"...\n", argv[2]);

                    ltx_Topic_subscribe(print_data_item_list[i].topic, &(print_data_item_list[i].subscriber));
                    return ;
                }
            }

            LOG_FMT(PRINT_WARNNING"Data name \"%s\" not matched! Use <-list> option to list all data name.\n", argv[2]);

            break;

        case PO_STOP:
            // LOG_STR(PRINT_DEBUG"going into PO_STOP\n");

            for(i = 0; print_data_item_list[i].item_name[0] != ' '; i ++){
                ltx_Topic_unsubscribe(print_data_item_list[i].topic, &(print_data_item_list[i].subscriber));
            }
            LOG_STR(PRINT_LOG"All data tracking has been stopped.\n");

            break;

        case PO_LIST:
            // LOG_STR(PRINT_DEBUG"going into PO_LIST\n");

            LOG_STR(PRINT_LOG"All data item name:\n");
            for(i = 0; print_data_item_list[i].item_name[0] != ' '; i ++){
                LOG_FMT("\t%s\n", print_data_item_list[i].item_name);
            }

            break;

        default:
            LOG_FMT(PRINT_WARNNING"Unknown option: %s\n", argv[1]);

            goto Useage_print;
            break;
    }

    return ;

Useage_print:
    LOG_FMT(PRINT_LOG"Useage: %s <-start/-stop/-list> [data_name]\n", argv[0]);
}

// 读写某些参数的命令
void cmd_cb_param(uint8_t argc, char *argv[]){
    if(argc < 2){
        goto Useage_param;
    }

    switch(argv[1][1]){
        case 'r':
            if(argc < 3){
                LOG_STR(PRINT_WARNNING"Please enter param name! You can use -l option to list all param.\n");
                return ;
            }

            for(uint8_t i = 0; param_list[i].param_name[0] != ' '; i ++){
                if(my_str_cmp(param_list[i].param_name, argv[2]) == 0){
                    param_list[i].param_read(&param_list[i]);

                    return ;
                }
            }

            LOG_FMT(PRINT_WARNNING"Param '%s' was not found, use -l to list all param\n", argv[2]);

            break;

        case 'w':
            if(argc < 4){
                LOG_STR(PRINT_WARNNING"Please enter param name and new value! You can use -l option to list all param.\n");
                return ;
            }

            for(uint8_t i = 0; param_list[i].param_name[0] != ' '; i ++){
                if(my_str_cmp(param_list[i].param_name, argv[2]) == 0){
                    param_list[i].param_write(&param_list[i], argv[3]);

                    return ;
                }
            }

            LOG_FMT(PRINT_WARNNING"Param '%s' was not found, use -l to list all param\n", argv[2]);

            break;

        case 'l':
            LOG_STR(PRINT_LOG"All rw-able parameter:\n");
            for(uint8_t i = 0; param_list[i].param_name[0] != ' '; i ++){
                LOG_FMT(PRINT_LOG"\t%s\n", param_list[i].param_name);
            }

            break;

        default:
            LOG_FMT(PRINT_WARNNING"Unknown option: %s\n", argv[1]);
            goto Useage_param;
            break;
    }

    return ;
    
Useage_param:
    LOG_FMT(PRINT_LOG"Useage: %s <-r/-s/-l> [<param_name> [new_value]]\n", argv[0]);
    LOG_STR(PRINT_LOG"\t-r: read, -w: write, -l: list\n");
}


// 处理输入，执行命令
void ltx_Cmd_process(char *cmd){
    uint32_t i = 0;
    uint8_t index = 1;
    char *argv[CMD_MAX_ARG_COUNTS] = {
        [0] = cmd + 1,
    };

    if(!(cmd[0] == '/' || cmd[0] == '#')){ // 非指令
        LOG_FMT(PRINT_WARNNING"Unknow format!\n");
        // LOG_FMT(PRINT_DEBUG"%s\n", cmd);
        return;
    }

    for(i = 1; (cmd[i] != '\0') && (i < CMD_BUF_SIZE - 1) && (index < CMD_MAX_ARG_COUNTS - 1); i ++){
        if((cmd[i-1] == ' ') && (cmd[i] != ' ')){
            argv[index] = &cmd[i];
            index ++;
            cmd[i-1] = '\0';
        }
    }

    for(i = 0; cmd_list[i].cmd_name[0] != ' '; i ++){
        if(my_str_cmp(cmd_list[i].cmd_name, argv[0]) == 0){
            if(cmd_list[i].cmd_cb != NULL){
                argv[0] = cmd;
                cmd_list[i].cmd_cb(index, argv);
            }else {
                LOG_FMT(PRINT_ERROR"Callback function of \"%s\" is not define!\n", argv[0]);
            }
            return;
        }
    }

    LOG_FMT(PRINT_WARNNING"Unknown cmd: %s\n", argv[0]);
    LOG_FMT(PRINT_LOG"Type /help to list all commands\n");
}
