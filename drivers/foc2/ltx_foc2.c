#include "ltx_foc2.h"

#define SQRT3DIV2           0.8660254037844386467637231707529f
#define SQRT3               1.7320508075688772935274463415059f

// 以百分比形式输入输出，内部不关心母线电压
void ltx_foc2_svpwm_1(float V_alpha, float V_beta, float V_outputABC[]){
    // 1. 计算扇区
    float V1 = V_beta;
    float V2 = -0.5f * V_beta + 0.5f * SQRT3 * V_alpha;
    float V3 = -0.5f * V_beta - 0.5f * SQRT3 * V_alpha;
    
    uint8_t sector = 0;
    if (V1 > 0) sector = 1;
    if (V2 > 0) sector = sector + 2;
    if (V3 > 0) sector = sector + 4;
    // sector 值为 1~6，对应扇区 I~VI
    
    // 2. 计算 X, Y, Z (T1, T2 的中间变量)
    float T = 1.0f;  // 假设 PWM 周期标幺为 1，实际占空比在 0~1 之间
    float X =  V_alpha * T;
    float Y = (0.5f * V_alpha + 0.5f * SQRT3 * V_beta) * T;
    float Z = (0.5f * V_alpha - 0.5f * SQRT3 * V_beta) * T;
    
    float T1, T2;
    switch (sector) {
        case 1: // 扇区 I: (0°~60°) 矢量 4-6
            T1 =  Z;
            T2 =  Y;
            break;
        case 2: // 扇区 II: (60°~120°) 矢量 6-2
            T1 =  Y;
            T2 = -X;
            break;
        case 3: // 扇区 III: (120°~180°) 矢量 2-3
            T1 = -Z;
            T2 =  X;
            break;
        case 4: // 扇区 IV: (180°~240°) 矢量 3-1
            T1 = -X;
            T2 =  Z;
            break;
        case 5: // 扇区 V: (240°~300°) 矢量 1-5
            T1 =  X;
            T2 = -Y;
            break;
        case 6: // 扇区 VI: (300°~360°) 矢量 5-4
            T1 = -Y;
            T2 = -Z;
            break;
        default:
            T1 = 0; T2 = 0;
    }
    
    // 3. 过调制处理
    float Ts = T1 + T2;
    if (Ts > T) {
        float scale = T / Ts;
        T1 *= scale;
        T2 *= scale;
    }
    
    // 4. 计算三相占空比（中心对齐模式，比较值）
    float Ta = (T - T1 - T2) * 0.5f;
    float Tb = Ta + T1;
    float Tc = Tb + T2;
    
    float d1, d2, d3;
    switch (sector) {
        case 1:
            d1 = Tb; d2 = Ta; d3 = Tc; // A相 B相 C相 对应关系需根据硬件确认
            break;
        case 2:
            d1 = Ta; d2 = Tc; d3 = Tb;
            break;
        case 3:
            d1 = Ta; d2 = Tb; d3 = Tc;
            break;
        case 4:
            d1 = Tc; d2 = Tb; d3 = Ta;
            break;
        case 5:
            d1 = Tc; d2 = Ta; d3 = Tb;
            break;
        case 6:
            d1 = Tb; d2 = Tc; d3 = Ta;
            break;
        default:
            d1 = Ta; d2 = Ta; d3 = Ta; // 零矢量
    }
    
    // 输出占空比（0~1）
    V_outputABC[0] = d1;
    V_outputABC[1] = d2;
    V_outputABC[2] = d3;
}

// 以百分比形式输入输出，内部不关心母线电压
// 输入条件：sqrt(V_alpha*V_alpha + V_beta*V_beta) < 1/sqrt(3)
void ltx_foc2_svpwm_2(float V_alpha, float V_beta, float V_outputABC[]){
    
    // 将Vα,Vβ转换为三相占空比（标幺化，范围0-1）
    // 首先计算中间变量
    float u = V_alpha;
    float v = 0.5f * (-V_alpha + SQRT3 * V_beta);
    float w = 0.5f * (-V_alpha - SQRT3 * V_beta);
    
    // 求最大值和最小值
    float umin = fminf(u, fminf(v, w));
    float umax = fmaxf(u, fmaxf(v, w));
    
    // 中心偏移
    float offset = 0.5f * (umin + umax);
    
    // 得到占空比（标幺化到0~1）
    V_outputABC[0] = u - offset + 0.5f;
    V_outputABC[1] = v - offset + 0.5f;
    V_outputABC[2] = w - offset + 0.5f;
    
    // 限幅
    if(V_outputABC[0] < 0) V_outputABC[0] = 0; if(V_outputABC[0] > 1) V_outputABC[0] = 1;
    if(V_outputABC[1] < 0) V_outputABC[1] = 0; if(V_outputABC[1] > 1) V_outputABC[1] = 1;
    if(V_outputABC[2] < 0) V_outputABC[2] = 0; if(V_outputABC[2] > 1) V_outputABC[2] = 1;
}

// 以百分比形式输入输出，内部不关心母线电压
// 输入条件：sqrt(V_alpha*V_alpha + V_beta*V_beta) < 1/sqrt(3)
// 满足输入条件的情况下，最高只输出母线电压 97%，避免百分百占空比无法低侧电流采样
void ltx_foc2_svpwm_3(float V_alpha, float V_beta, float V_outputABC[]){
    
    // 将Vα,Vβ转换为三相占空比（标幺化，范围0-1）
    // 首先计算中间变量
    float u = V_alpha;
    float v = 0.5f * (-V_alpha + SQRT3 * V_beta);
    float w = 0.5f * (-V_alpha - SQRT3 * V_beta);
    
    // 求最大值和最小值
    float umin = fminf(u, fminf(v, w));
    float umax = fmaxf(u, fmaxf(v, w));
    
    // 中心偏移
    float offset = 0.5f * (umin + umax);
    
    // 得到占空比（标幺化到0~1）
    V_outputABC[0] = u - offset + 0.5f;
    V_outputABC[1] = v - offset + 0.5f;
    V_outputABC[2] = w - offset + 0.5f;
    
    // 限幅
    if(V_outputABC[0] < 0) V_outputABC[0] = 0; if(V_outputABC[0] > 1) V_outputABC[0] = 1;
    if(V_outputABC[1] < 0) V_outputABC[1] = 0; if(V_outputABC[1] > 1) V_outputABC[1] = 1;
    if(V_outputABC[2] < 0) V_outputABC[2] = 0; if(V_outputABC[2] > 1) V_outputABC[2] = 1;

    V_outputABC[0] *= 0.97f;
    V_outputABC[1] *= 0.97f;
    V_outputABC[2] *= 0.97f;
}
