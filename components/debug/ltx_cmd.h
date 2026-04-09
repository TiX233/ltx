/**
 * @file ltx_cmd.h
 * @author realTiX
 * @brief 对上位机发来的命令进行处理用
 * @version 3.1
 * @date 2025-08-16 (0.2)
 *       2025-11-24 (0.3，纳入 ltx 可选组件)
 * 
 *       2026-01-13 (2.0，适配 ltx V2)
 * 
 *       2026-01-28 (3.0，适配 ltx V3)
 *       2026-03-11 (3.1，优化 print 命令创建打印对象方法；修改 param 命令适配 vofa 多出来的冒号)
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef __LTX_CMD_H__
#define __LTX_CMD_H__

#define CMD_MAX_ARG_COUNTS      10
#define CMD_BUF_SIZE            128

void ltx_Cmd_process(char *cmd);

#endif // __LTX_CMD_H__
