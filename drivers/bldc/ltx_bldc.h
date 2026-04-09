/**
 * @file ltx_bldc.h
 * @author realTiX
 * @brief 用于三相无刷电机的驱动库。为了提供 （静态）多实例能力+快速响应+便于创建用户回调，这里通过宏实现了用户自定义内联函数回调，而不是使用一般的结构体成员函数回调
 * @version 0.1
 * @date 2026-02-15 (0.1, 初步完成)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_BLDC_H__
#define __LTX_BLDC_H__

#include "ltx.h"

typedef uint32_t adc_type_t; // 设定 adc 原始数据类型
// typedef uint16_t adc_type_t; // 设定 adc 原始数据类型

struct ltx_bldc_stu {
    uint8_t id; // 暂时没什么用

    float current_u;
    float current_v;
    float current_w;
};

// 样例：创建两个电机对象
// struct ltx_bldc_stu motor1 = {.id = 1};
// struct ltx_bldc_stu motor2 = {.id = 2};

// ================== 配置 设置 pwm 占空比 用户内联回调宏 ==================
#define ltx_bldc_config_duty_u_cb(motor, code)\
    ltx_inline void motor##_set_duty_u(float duty) code
#define ltx_bldc_config_duty_v_cb(motor, code)\
    ltx_inline void motor##_set_duty_v(float duty) code
#define ltx_bldc_config_duty_w_cb(motor, code)\
    ltx_inline void motor##_set_duty_w(float duty) code

// 样例：创建一个内联函数作为 motor1 的设置 u 路 pwm 占空比回调：
// ltx_bldc_config_duty_u_cb(motor1, {
//     TIM1->CCR1 = (uint32_t)(duty*1234); // 直接操作寄存器，响应更快
//     printf("set motor1 u duty to %d", duty); // 打印
// })
// 创建一个内联函数作为 motor2 的设置 w 路 pwm 占空比回调：
// ltx_bldc_config_duty_w_cb(motor2, {
//     __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, duty*5678); // 调用其他库
// })

// ================== 设置占空比 api ==================
#define ltx_bldc_set_duty_u(motor, duty)\
    motor##_set_duty_u(duty)
#define ltx_bldc_set_duty_v(motor, duty)\
    motor##_set_duty_v(duty)
#define ltx_bldc_set_duty_w(motor, duty)\
    motor##_set_duty_w(duty)

// 使用样例：
// ltx_bldc_set_duty_u(motor1, 0.5); // 设置电机 1 的 u 路 pwm 占空比为 50%
// ltx_bldc_set_duty_w(motor2, 0.72); // 设置电机 2 的 w 路 pwm 占空比为 72%


#if 0 // 不太灵活，废弃
// ================== 配置 获取电流 用户内联回调宏 ==================
#define ltx_bldc_config_get_current_u_cb(motor, code)\
    ltx_inline float motor##_get_current_u(void) code
#define ltx_bldc_config_get_current_v_cb(motor, code)\
    ltx_inline float motor##_get_current_v(void) code
#define ltx_bldc_config_get_current_w_cb(motor, code)\
    ltx_inline float motor##_get_current_w(void) code

// 样例：创建一个内联函数作为 motor1 的获取 u 路电流回调：
// ltx_bldc_config_get_current_u_cb(motor1, {
//     return ADC1->DR*0.1234;
// })
// 创建一个内联函数作为 motor2 的获取 w 路电流回调：
// ltx_bldc_config_get_current_w_cb(motor2, {
//     return adc_data[2];
// })

// ================== 获取电流 api ==================
#define ltx_bldc_get_current_u(motor)\
    motor##_get_current_u()
#define ltx_bldc_get_current_v(motor)\
    motor##_get_current_v()
#define ltx_bldc_get_current_w(motor)\
    motor##_get_current_w()

// 使用样例：
// float current1u = ltx_bldc_get_current_u(motor1); // 获取电机 1 的 u 路电流
// float current2w = ltx_bldc_get_current_w(motor2); // 获取电机 2 的 w 路电流
#endif

// ================== 配置 转换电流 用户内联回调宏 ==================
#define ltx_bldc_config_trans_current_u_cb(motor, code)\
    ltx_inline void motor##_trans_current_u(adc_type_t adc_val) code
#define ltx_bldc_config_trans_current_v_cb(motor, code)\
    ltx_inline void motor##_trans_current_v(adc_type_t adc_val) code
#define ltx_bldc_config_trans_current_w_cb(motor, code)\
    ltx_inline void motor##_trans_current_w(adc_type_t adc_val) code

// 样例：创建一个内联函数作为 motor1 的转换 u 路 adc 数值为电流回调：
// ltx_bldc_config_trans_current_u_cb(motor1, {
//     motor1.current_u = adc_val * 0.1234; // 转换好后可以存储在对象成员变量中
//     printf("motor1 u current now: %f", motor1.current_u); // 打印
// })
// 创建一个内联函数作为 motor2 的转换 w 路 adc 数值为电流回调：
// ltx_bldc_config_trans_current_w_cb(motor2, {
//     motor2.current_w = adc_val * 0.5678; // 转换好后可以存储在对象成员变量中
// })

// ================== 转换电流 api ==================
#define ltx_bldc_trans_current_u(motor, adc_val)\
    motor##_trans_current_u(adc_val)
#define ltx_bldc_trans_current_v(motor, adc_val)\
    motor##_trans_current_v(adc_val)
#define ltx_bldc_trans_current_w(motor, adc_val)\
    motor##_trans_current_w(adc_val)

// 使用样例：
// ltx_bldc_trans_current_u(motor1, adc1_buffer[0]); // 转换 adc 原始数据到电机 1 的 u 路电流
// ltx_bldc_trans_current_w(motor2, adc2_buffer[2]); // 转换 adc 原始数据到电机 2 的 w 路电流

#endif // __LTX_BLDC_H__
