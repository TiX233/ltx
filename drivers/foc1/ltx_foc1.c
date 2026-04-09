#include "ltx_foc1.h"

// 这里我们将六边形内切圆半径作为单位 1
// 需要将输出限制在六边形内切圆范围内（不能超过 1），否则输出会被截顶
// 内切圆半径为六边形边长的 sqrt(3)/2 倍
#define SQRT3DIV2           0.8660254037844386467637231707529f
#define SQRT3               1.7320508075688772935274463415059f

// 60 度的弧度值
#define ANGLE60_TO_RAD      1.0471975511965977461542144610932f

// 六向量 表，对于 flash 有等待的情况下，似乎不用 const 会好一点？
const 
uint8_t six_vector_list[6][3] = {
    [0] = {1, 0, 0},
    [1] = {1, 1, 0},
    [2] = {0, 1, 0},
    [3] = {0, 1, 1},
    [4] = {0, 0, 1},
    [5] = {1, 0, 1},
};

// 可以自行设置两个零向量中 U1 的占比(0~1)：
static float u1_u0 = 0.5f; // 默认设置为对半开
/**
 * @brief 零矢量比例可变的 svpwm 算法，设置为 0.5 时表示 U1 和 U0 对半开，此时谐波最小
 * @param V_amplitude: 电压向量幅值百分比，[0~1]
 * @param V_rad:       电压向量弧度，逆时针，[0, 2pi)，电角度
 * @param V_outputABC: 输出三相电压占空比
 */
void ltx_foc1_svpwm_vec10(float V_amplitude, float V_rad, float V_outputABC[]){
    
    // 扇区
    uint8_t sector;
    uint8_t sector_next;
    // 扇区内的角度
    float theta; // θ = V_rad % 60度
    float k1, k2; // 左右向量系数
    float cos_theta, sin_theta;
    // 左右向量以外的零向量可分配的时间
    float t_zero;
    // 零向量中 U1 可占有的时间占比，U0 不用管，因为是 0，所以数值上相当于没加
    float t_zero_u1;

    // θ = V_rad % 60度
    theta = fmodf(V_rad, ANGLE60_TO_RAD);

    // 计算扇区
    sector = (uint8_t)(V_rad / ANGLE60_TO_RAD); // 不用条件判断，直接抹小数
    sector_next = (sector + 1)%6;

    // 向量合成
    cos_theta = cosf(theta);
    sin_theta = sinf(theta);

    // 计算左右两个基向量的时间占比
    // k1 = V_amplitude*(cos_theta - sin_theta/sqrt(3)) * SQRT3DIV2;
    // k2 = 2*V_amplitude*sin_theta/sqrt(3) * SQRT3DIV2;
    k1 = V_amplitude/2*(SQRT3 * cos_theta - sin_theta);
    k2 = V_amplitude*sin_theta;

    // 计算可分配给零向量的时间占比
    t_zero = 1.0f - k1 - k2; // t_zero 不会小于零，读者自证不难

    // 计算 U1 可分配的时间百分比
    t_zero_u1 = u1_u0 * t_zero;

    // 加权平均，t_zero_u1 * 1 = t_zero_u1，t_zero_u0 * 0 = 0
    V_outputABC[0] = k1 * six_vector_list[sector][0] + k2 * six_vector_list[sector_next][0] + t_zero_u1;
    V_outputABC[1] = k1 * six_vector_list[sector][1] + k2 * six_vector_list[sector_next][1] + t_zero_u1;
    V_outputABC[2] = k1 * six_vector_list[sector][2] + k2 * six_vector_list[sector_next][2] + t_zero_u1;
}

/**
 * @brief 零矢量全使用 U0 的 svpwm 算法，6 次谐波稍大，但是开关损耗小三分之一
 * @param V_amplitude: 电压向量幅值百分比，[0~1]
 * @param V_rad:       电压向量弧度，逆时针，[0, 2pi)，电角度
 * @param V_outputABC: 输出三相电压占空比
 */
void ltx_foc1_svpwm_vec0(float V_amplitude, float V_rad, float V_outputABC[]){
    
    // 扇区
    uint8_t sector;
    uint8_t sector_next;
    // 扇区内的角度
    float theta; // θ = V_rad % 60度
    float k1, k2; // 左右向量系数
    float cos_theta, sin_theta;
    // 左右向量以外的零向量可分配的时间

    // θ = V_rad % 60度
    theta = fmodf(V_rad, ANGLE60_TO_RAD);

    // 计算扇区
    sector = (uint8_t)(V_rad / ANGLE60_TO_RAD); // 不用条件判断，直接抹小数
    sector_next = (sector + 1)%6;

    // 向量合成
    cos_theta = cosf(theta);
    sin_theta = sinf(theta);

    // 计算左右两个基向量的时间占比
    // k1 = V_amplitude*(cos_theta - sin_theta/sqrt(3)) * SQRT3DIV2;
    // k2 = 2*V_amplitude*sin_theta/sqrt(3) * SQRT3DIV2;
    k1 = V_amplitude/2*(SQRT3 * cos_theta - sin_theta);
    k2 = V_amplitude*sin_theta;

    // 加权平均，t_zero_u1 * 1 = t_zero_u1，t_zero_u0 * 0 = 0
    V_outputABC[0] = k1 * six_vector_list[sector][0] + k2 * six_vector_list[sector_next][0];
    V_outputABC[1] = k1 * six_vector_list[sector][1] + k2 * six_vector_list[sector_next][1];
    V_outputABC[2] = k1 * six_vector_list[sector][2] + k2 * six_vector_list[sector_next][2];
}
