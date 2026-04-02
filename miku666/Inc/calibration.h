/*
 * calibration.h
 *
 * 用于MCP4725 DAC控制蠕动泵的转速校准
 * 2026-01-27 Updated: 支持在线校准模式
 */

#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化校准模块 (加载默认参数)
 */
void Calibration_Init(void);

/**
 * @brief  根据目标转速，计算需要发送给DAC的修正后的指令值
 * @param  target_speed: 期望的实际转速 (0-100)
 * @return float: 修正后的输入指令值 (0-100)，用于计算DAC电压
 */
float Calibration_GetCorrectedValue(float target_speed);

// --- 在线校准 API ---

/**
 * @brief  开始在线校准 (清空临时缓冲区)
 */
void Calibration_Start(void);

/**
 * @brief  添加一个校准点
 * @param  command_val: 发送给DAC的指令值 (0-100)
 * @param  measured_val: 实际测量到的转速
 * @return int: 1=成功, 0=失败(缓冲区满)
 */
int Calibration_AddPoint(float command_val, float measured_val);

/**
 * @brief  结束校准，对数据排序并应用到系统
 * @return int: 采集到的有效点数
 */
int Calibration_End(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H_ */
