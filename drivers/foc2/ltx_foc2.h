/**
 * @file ltx_foc2.h
 * @author realTiX
 * @brief foc 算法库2，目标输出以 dp 形式传入
 * @version 0.1
 * @date 2026-03-14 (0.1，初步完成功能设计)
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __LTX_FOC2_H__
#define __LTX_FOC2_H__

#include "ltx.h"
#include "ltx_pid.h"
#include "math.h"

#ifndef PI
    #define PI  3.14159265358979f
#endif

struct ltx_foc2_stu {
    uint8_t flag_is_inited; // 初始化标志位，设为 1 才会进一步调用 pid 控制器，设为 0 仅计算电流向量不操作输出

    uint8_t pole_pairs; // 极对数

    float I_q; // 当前电流向量模长，由 foc 算法填入，外部只应该读
    float I_d; // 当前电流向量弧度，[0, 2pi)，由 foc 算法填入，外部只应该读
    
    float V_aplha; // 当前输出电压向量模长百分比，[0~1]，由 foc 算法填入，外部只应该读
    float V_beta; // 当前输出电压向量弧度，[0, 2pi)，由 foc 算法填入，外部只应该读

    struct ltx_pid_pi_stu pi_q; // q 轴 pi 控制器
    struct ltx_pid_pi_stu pi_d; // d 轴 pi 控制器

    // 三相电压输出，调用 foc 算法后得出，用户可根据是否初始化来决定是否输出
    float v_outputABC[3]; // svpwm 算法根据所需电压向量的模长与弧度得出的三相电压输出
    // 三相采集电流，由外部在调用 foc 算法前填入
    float i_A;
    float i_B;
    float i_C;

    // 由用户设定的 dq 轴目标电流
    float target_I_q;
    float target_I_d;

    // 当前目标电流与实际输出电流的差值，外部只应该读
    float diff_I_q;
    float diff_I_d;

    float rotor_rad; // 转子机械弧度，[0, 2pi)，外部填入，需要与电角度对齐零点，不需要额外乘以极对数，foc 算法会处理

    float dt; // foc 算法调用间隔，单位默认毫秒，可以删去，直接用 k 转换量纲
};

// svpwm 生成算法，任选其一
void ltx_foc2_svpwm_1(float V_alpha, float V_beta, float V_outputABC[]); // 扇区判断版本，未验证
void ltx_foc2_svpwm_2(float V_alpha, float V_beta, float V_outputABC[]); // 简化等效版本，可输出 100% 电压
void ltx_foc2_svpwm_3(float V_alpha, float V_beta, float V_outputABC[]); // 简化等效版本，最高只输出 97% 母线电压，避免低侧电阻无法采样

#define ltx_foc2_svpwm_algorithm    ltx_foc2_svpwm_3


// foc 算法，按 dt 定期调用，一般在 adc 采样完成回调调用，adc 一般配置为 pwm 周期触发
// 调用前需要用户填入 iABC，调用后由用户决定是否输出 vABC
ltx_inline void ltx_foc2_algorithm(struct ltx_foc2_stu *_foc_obj){
    
    // 三相电流克拉克变换得到 iαβ，外部要对三相电流取负
    float i_alpha = _foc_obj->i_A;
    float i_beta = (_foc_obj->i_A + 2.0f * _foc_obj->i_B) * 0.577350269f;
    
    // 转换电角度
    float elec_rad = _foc_obj->rotor_rad * _foc_obj->pole_pairs;
    
    // 将 iαβ 通过帕克变换转到 iDQ
    float c = cosf(elec_rad);
    float s = sinf(elec_rad);
    _foc_obj->I_q = -i_alpha * s + i_beta * c;
    _foc_obj->I_d =  i_alpha * c + i_beta * s;
    
    // 计算 qd 轴电流误差
    _foc_obj->diff_I_q = _foc_obj->target_I_q - _foc_obj->I_q;
    _foc_obj->diff_I_d = _foc_obj->target_I_d - _foc_obj->I_d;
    
    // 误差输入给 pi 控制器
    float vq = ltx_pid_pi_update(&_foc_obj->pi_q, _foc_obj->diff_I_q, _foc_obj->dt);
    float vd = ltx_pid_pi_update(&_foc_obj->pi_d, _foc_obj->diff_I_d, _foc_obj->dt);
    
    // 逆帕克变换得到 Vαβ
    _foc_obj->V_aplha = vd * c - vq * s;
    _foc_obj->V_beta  = vd * s + vq * c;

    // 使用 Vαβ 计算三相 svpwm 输出
    ltx_foc2_svpwm_algorithm(_foc_obj->V_aplha, _foc_obj->V_beta, _foc_obj->v_outputABC);
}

#endif // __LTX_FOC2_H__
