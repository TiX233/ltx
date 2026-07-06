/**
 * @file ltx_foc1.h
 * @author realTiX
 * @brief foc 算法库1，目标向量以极坐标形式输入，效果不好，暂不计划继续深入研究，请使用 foc2 库。本库只用于强拖
 * @version 0.1
 * @date 2026-03-11 (0.1，初步完成功能设计)
 * 
 * @copyright Copyright (c) 2026, realTiX
 * @license GPL-3.0
 * 
 * SPDX-FileCopyrightText: 2026 realTiX
 * SPDX-License-Identifier: GPL-3.0-only
 * 
 */
#ifndef __LTX_FOC1_H__
#define __LTX_FOC1_H__

#include "ltx.h"
#include "ltx_pid.h"
#include "math.h"

#ifndef PI
    #define PI  3.14159265358979f
#endif

struct ltx_foc1_stu {
    uint8_t flag_is_inited; // 初始化标志位，设为 1 才会进一步调用 pid 控制器，设为 0 仅计算电流向量不操作输出

    uint8_t pole_pairs; // 极对数

    float vector_I_len; // 当前电流向量模长，由 foc 算法填入，外部只应该读
    float vector_I_rad; // 当前电流向量弧度，[0, 2pi)，由 foc 算法填入，外部只应该读
    
    float vector_V_len; // 电压向量模长百分比，[0~1]，由 foc 算法填入，外部只应该读
    float vector_V_rad; // 电压向量弧度，[0, 2pi)，由 foc 算法填入，外部只应该读

    struct ltx_pid_pi_stu pi_amplitude; // 电流模长环，limit 不应该超过 1，即 100% 输出电压占空比
    // 按照 microcai 的文章，他是直接前馈相位没用 pi，还没试
    struct ltx_pid_pi_angle_stu pi_theta; // 电流与转子的夹角环，其实不应该专门设计一个角度 pi 控制器，应该直接写一个通用增量式 pi 控制器

    // 三相电压输出
    float v_outputABC[3]; // svpwm 算法根据所需电压向量的模长与弧度得出的三相电压输出
    // 三相采集电流
    float i_A;
    float i_B;
    float i_C;

    float target_I_len; // 目标输出电流向量模长，可取负，通常上层只需要管这个就可以了
    float target_I_rad; // 目标输出电流向量与转子方向夹角，[0, PI]，顺逆时针以电流模长正负来判断，一般设置为 PI/2，即 id = 0 控制，上层一般不需要操作它

    float diff_len; // 目标输出电流向量模长与实际输出模长的差，由 foc 算法填入，外部只应该读
    float diff_rad; // 实际输出电流向量与转子方向夹角，由 foc 算法填入，外部只应该读

    float rotor_rad; // 转子机械弧度，[0, 2pi)，外部填入，需要与电角度对齐零点，不需要额外乘以极对数，foc 算法会处理

    float dt; // foc 算法调用间隔，单位默认毫秒
};

// svpwm 生成算法，任选其一
void ltx_foc1_svpwm_vec10(float V_amplitude, float V_rad, float V_outputABC[]); // U(111) 和 U(000) 按照特定比例分配零向量，默认平均分配
void ltx_foc1_svpwm_vec0(float V_amplitude, float V_rad, float V_outputABC[]); // 全使用 U0 作为零向量，开关损耗更小，但六次谐波稍大

#define ltx_foc1_svpwm_algorithm    ltx_foc1_svpwm_vec10


// foc 算法，按 dt 定期调用，一般在 adc 采样完成回调调用，adc 一般配置为 pwm 周期触发
// 调用前需要用户填入 iABC，调用后由用户决定是否输出 vABC

// 版本 1，如果要求输出负向量则直接将目标角度镜像
ltx_inline void ltx_foc1_algorithm_1(struct ltx_foc1_stu *_foc_obj){
    float ri_rad; // 转子匹配电角度
    
    // 计算电流向量模长
    _foc_obj->vector_I_len = sqrtf(2.0f/3 * (_foc_obj->i_A * _foc_obj->i_A + _foc_obj->i_B * _foc_obj->i_B + _foc_obj->i_C * _foc_obj->i_C));

    // 计算电流向量弧度
    // _foc_obj->vector_I_rad = acosf(_foc_obj->i_A / _foc_obj->vector_I_len);
    // if(_foc_obj->i_B < _foc_obj->i_C) _foc_obj->vector_I_rad = (2*PI) - _foc_obj->vector_I_rad;

    // 钳位避免超越定义域 [-1, 1]
    float ratio = _foc_obj->i_A / _foc_obj->vector_I_len;
    // ratio = (ratio > 1.0f) ? 1.0f : ((ratio < -1.0f) ? -1.0f : ratio);
    // 范围超出那就直接赋值，不用额外算反余弦
    if(ratio < -1.0f){
        _foc_obj->vector_I_rad = PI;
    }else if(ratio > 1.0f){
        if(_foc_obj->i_B < _foc_obj->i_C){
            _foc_obj->vector_I_rad = 2*PI;
        }else {
            _foc_obj->vector_I_rad = 0.0f;
        }
    }else {
        _foc_obj->vector_I_rad = acosf(ratio);
        if(_foc_obj->i_B < _foc_obj->i_C) _foc_obj->vector_I_rad = (2*PI) - _foc_obj->vector_I_rad;
    }


    if(_foc_obj->vector_I_len < 0.006f){ // 输出较小，此时噪声占比会明显影响角度判断
        // 角度可以不用管，等模长增长起来一点再管也来得及，或者这里也可以直接输出目标相位
        if(_foc_obj->target_I_len < 0){ // 要求输出顺时针夹角向量
            _foc_obj->vector_V_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs - _foc_obj->target_I_rad, (2*PI));
            if(_foc_obj->vector_V_rad < 0) _foc_obj->vector_V_rad += 2*PI;
        }else { // 要求输出逆时针夹角向量
            _foc_obj->vector_V_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs + _foc_obj->target_I_rad, (2*PI));
        }
    }else { // 输出强度足够判断电流向量当前方向

        // 转换转子角度匹配电角度：r*7 %2PI，并且加上目标夹角，如果要求输出负向量，那么就相当于镜像角度
        if(_foc_obj->target_I_len < 0){
            ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs - _foc_obj->target_I_rad, (2*PI));
            if(ri_rad < 0) ri_rad += 2*PI;
        }else{
            ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs + _foc_obj->target_I_rad, (2*PI));
        }
        // 对比电流向量方向与目标角度的偏差
        _foc_obj->diff_rad = ri_rad - _foc_obj->vector_I_rad;
        // 归一到 [-PI, PI]，避免反转
        if(_foc_obj->diff_rad < -PI){
            _foc_obj->diff_rad += (2*PI);
        }else if(_foc_obj->diff_rad > PI){
            _foc_obj->diff_rad -= (2*PI);
        }
        // 将角度偏差值输入 pi 控制器，获取 svpwm 下次输出的电压角度
        _foc_obj->vector_V_rad = ltx_pid_pi_angle_update(&(_foc_obj->pi_theta), _foc_obj->diff_rad, _foc_obj->dt);
    }
    _foc_obj->diff_len = fabsf(_foc_obj->target_I_len) - _foc_obj->vector_I_len;

    // 将模长偏差输入 pi 控制器，获取 svpwm 下次输出的电压模长
    _foc_obj->vector_V_len = ltx_pid_pi_update(&(_foc_obj->pi_amplitude), _foc_obj->diff_len, _foc_obj->dt);

    // 计算 svpwm 输出
    ltx_foc1_svpwm_algorithm(_foc_obj->vector_V_len, _foc_obj->vector_V_rad, _foc_obj->v_outputABC);
}

// 版本 2，如果要求输出负向量则先减低模长到 0，再镜像角度后增减模长，有 bug
ltx_inline void ltx_foc1_algorithm_2(struct ltx_foc1_stu *_foc_obj){
    float ri_rad; // 转子匹配电角度
    float t_rad; // 中间变量
    
    // 计算电流向量模长
    _foc_obj->vector_I_len = sqrtf(2.0f/3 * (_foc_obj->i_A * _foc_obj->i_A + _foc_obj->i_B * _foc_obj->i_B + _foc_obj->i_C * _foc_obj->i_C));

    // 计算电流向量弧度
    // 钳位避免超越定义域 [-1, 1]
    float ratio = _foc_obj->i_A / _foc_obj->vector_I_len;
    // 范围超出那就直接赋值，不用额外算反余弦
    if(ratio < -1.0f){
        _foc_obj->vector_I_rad = PI;
    }else if (ratio > 1.0f){
        if(_foc_obj->i_B < _foc_obj->i_C){
            _foc_obj->vector_I_rad = 2*PI;
        }else {
            _foc_obj->vector_I_rad = 0.0f;
        }
    }else {
        _foc_obj->vector_I_rad = acosf(ratio);
        if(_foc_obj->i_B < _foc_obj->i_C) _foc_obj->vector_I_rad = (2*PI) - _foc_obj->vector_I_rad;
    }


    if(_foc_obj->vector_I_len < 0.009f){ // 输出较小，此时噪声占比会明显影响角度判断
        // 直接用目标向量绝对值减当前值，角度可以不用管，等模长增长起来一点再管也来得及，或者这里也可以直接输出目标相位
        if(_foc_obj->target_I_len < 0){ // 要求输出顺时针向量
            _foc_obj->diff_len = -_foc_obj->target_I_len - _foc_obj->vector_I_len;

            // 直接输出目标相位，也可以省去，等增长模长后让夹角环 pi 控制器自己去调整
            _foc_obj->vector_V_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs - _foc_obj->target_I_rad, (2*PI));
            if(_foc_obj->vector_V_rad < 0) _foc_obj->vector_V_rad += 2*PI;
        }else { // 要求输出逆时针向量
            _foc_obj->diff_len = _foc_obj->target_I_len - _foc_obj->vector_I_len;
            _foc_obj->vector_V_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs + _foc_obj->target_I_rad, (2*PI));
        }
        // 将模长偏差输入 pi 控制器，获取 svpwm 下次输出的电压模长
        _foc_obj->vector_V_len = ltx_pid_pi_update(&(_foc_obj->pi_amplitude), _foc_obj->diff_len, _foc_obj->dt);
        
    }else { // 输出强度足够判断电流向量当前方向

        // 判断当前电流向量是转子的顺逆时针侧，t_rad > 0 则为逆时针侧
        t_rad = _foc_obj->vector_I_rad - fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs, (2*PI));
        if(t_rad < -PI){
            t_rad += (2*PI);
        }else if(t_rad > PI){
            t_rad -= (2*PI);
        }

        // 如果当前输出角度与目标向量同侧，直接增减模长即可
        // 如果当前输出角度与目标向量镜向侧，模长需要减少，等到接近零或者顺向后再增减
        if(_foc_obj->target_I_len < 0){ // 要求输出顺时针侧向量
            if(t_rad < 0){ // 正在输出顺时针侧向量，增减模长即可
                _foc_obj->diff_len = -_foc_obj->target_I_len - _foc_obj->vector_I_len;
                // 转换转子角度匹配电角度：r*7 %2PI，并且加上目标夹角
                ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs - _foc_obj->target_I_rad, (2*PI));
                if(ri_rad < 0) ri_rad += 2*PI;
                
            }else { // 正在输出逆时针向量，先减小模长
                _foc_obj->diff_len = _foc_obj->target_I_len - _foc_obj->vector_I_len;

                // 保持逆时针方向输出
                ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs + _foc_obj->target_I_rad, (2*PI));
            }
        }else { // 要求输出逆时针向量
            if(t_rad < 0){ // 正在输出顺时针向量，先减小模长
                _foc_obj->diff_len = _foc_obj->target_I_len - _foc_obj->vector_I_len;
                // 保持顺时针方向输出
                ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs - _foc_obj->target_I_rad, (2*PI));
                if(ri_rad < 0) ri_rad += 2*PI;
                
            }else { // 正在输出逆时针向量，增减模长即可
                _foc_obj->diff_len = -_foc_obj->target_I_len - _foc_obj->vector_I_len;
                ri_rad = fmodf(_foc_obj->rotor_rad * _foc_obj->pole_pairs + _foc_obj->target_I_rad, (2*PI));
            }
        }
        // 将模长偏差输入 pi 控制器，获取 svpwm 下次输出的电压模长
        _foc_obj->vector_V_len = ltx_pid_pi_update(&(_foc_obj->pi_amplitude), _foc_obj->diff_len, _foc_obj->dt);

        // 对比电流向量方向与目标角度的偏差
        _foc_obj->diff_rad = ri_rad - _foc_obj->vector_I_rad;
        // 归一到 [-PI, PI]，避免反转
        if(_foc_obj->diff_rad < -PI){
            _foc_obj->diff_rad += (2*PI);
        }else if(_foc_obj->diff_rad > PI){
            _foc_obj->diff_rad -= (2*PI);
        }
        // 将角度偏差值输入 pi 控制器，获取 svpwm 下次输出的电压角度
        _foc_obj->vector_V_rad = ltx_pid_pi_angle_update(&(_foc_obj->pi_theta), _foc_obj->diff_rad, _foc_obj->dt);
    }

    // 计算 svpwm 输出
    ltx_foc1_svpwm_algorithm(fabsf(_foc_obj->vector_V_len), _foc_obj->vector_V_rad, _foc_obj->v_outputABC);
}

// 设置目标输出电流向量长度，可为负
#define ltx_foc1_set_target_len(_foc_obj, len)      (_foc_obj.target_I_len = len)
// 设置目标输出电流向量与转子的夹角，[0, PI]，顺逆时针由输出的电流向量正负决定，一般只需要设置一次 PI/2 即为 id = 0 控制
// 实则不对，并不解耦，做不到 PI/2 情况下保证 id=0
#define ltx_foc1_set_target_rad(_foc_obj, rad)      (_foc_obj.target_I_rad = rad)


#endif // __LTX_FOC1_H__
