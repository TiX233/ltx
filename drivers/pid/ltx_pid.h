/**
 * @file ltx_pid.h
 * @author realTiX
 * @brief pid 库，目前仅有 pi 控制器。包含一个位置式 pi 控制器与一个角度专用增量式 pi 控制器
 * @version 0.1
 * @date 2026-03-10 (0.1，初步完成功能设计)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_PID_H__
#define __LTX_PID_H__

#include "ltx.h"
#include "math.h"

#ifndef PI
    #define PI  3.14159265358979f
#endif

// 位置式 pi 控制器对象结构体
struct ltx_pid_pi_stu {
    float kp;           // 比例
    float ki;           // 积分

    float integral;     // 积分量
    float limit_u;      // 输出上限
    float limit_d;      // 输出下限
};

// 更新位置式 pi 控制器 api
ltx_inline float ltx_pid_pi_update(struct ltx_pid_pi_stu *pi_stu, float error, float dt){
    pi_stu->integral += error * dt;
    // 抗积分饱和
    if(pi_stu->integral > pi_stu->limit_u) pi_stu->integral = pi_stu->limit_u;
    if(pi_stu->integral < pi_stu->limit_d) pi_stu->integral = pi_stu->limit_d;
    
    float output = pi_stu->kp * error + pi_stu->ki * pi_stu->integral;
    // 输出限幅
    if(output > pi_stu->limit_u) output = pi_stu->limit_u;
    if(output < pi_stu->limit_d) output = pi_stu->limit_d;
    return output;
}

// 角度 pi 控制器对象结构体，就是带取模的增量式 pi 控制器
struct ltx_pid_pi_angle_stu {
    float kp;           // 比例
    float ki;           // 积分

    float error_prev;   // 上一次误差（归一化到 [-π, π]）
    float theta;        // 当前输出电压角度（归一化到 [0, 2π)）
};

/**
 * @brief               角度 pi 更新函数
 * @param pi            控制器指针
 * @param error_rad     当前角度误差，输入范围 [-π, π]（由外部计算，例如目标电流角度减反馈电流角度）
 * @param dt            控制周期
 * @return              输出电压角度，归一化到 [0, 2π)
 */
ltx_inline float ltx_pid_pi_angle_update(struct ltx_pid_pi_angle_stu *pi, float error_rad, float dt){
    // 1. 计算误差变化量，并考虑角度周期性（防止 ±π 边界跳变）
    float delta_e = error_rad - pi->error_prev;
    if (delta_e > PI) delta_e -= 2.0f * PI;
    else if (delta_e < -PI) delta_e += 2.0f * PI;

    // 2. 增量式PI计算：输出角度变化量
    float delta = pi->kp * delta_e + pi->ki * error_rad * dt;

    // 3. 限幅：单步角度变化不超过π（防止反向旋转或过大冲击）
    if (delta > PI) delta = PI;
    else if (delta < -PI) delta = -PI;

    // 4. 更新累积角度
    pi->theta += delta;

    // 5. 归一化角度到 [0, 2π)
    pi->theta = fmodf(pi->theta, 2.0f * PI);
    if (pi->theta < 0) pi->theta += 2.0f * PI;

    // 6. 保存当前误差供下一周期使用
    pi->error_prev = error_rad;

    return pi->theta;
}

#endif // __LTX_PID_H__
