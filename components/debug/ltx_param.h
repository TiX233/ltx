/**
 * @file ltx_param.h
 * @author realTiX
 * @brief 用于读写一些预设的系统参数，由 ltx_cmd.c 中的 param 命令调用
 * @version 0.1
 * @date 2025-09-1  (0.1)
 *       2025-11-24 (0.2，纳入 ltx 可选组件)
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __LTX_PARAM_H__
#define __LTX_PARAM_H__

struct param_stu {
    const char *param_name;
    
    void (*param_read)(struct param_stu *param);
    void (*param_write)(struct param_stu *param, const char *new_val);
};

extern struct param_stu param_list[];

#endif // __LTX_PARAM_H__
