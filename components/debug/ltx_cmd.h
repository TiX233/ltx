/**
 * @file ltx_cmd.h
 * @author realTiX
 * @brief 对上位机发来的命令进行处理用
 * @version 0.3
 * @date 2025-08-16 (0.2)
 *       2025-11-24 (0.3，纳入 ltx 可选组件)
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __LTX_CMD_H__
#define __LTX_CMD_H__

#define CMD_MAX_ARG_COUNTS      10
#define CMD_BUF_SIZE            128

void ltx_Cmd_process(char *cmd);

#endif // __LTX_CMD_H__
