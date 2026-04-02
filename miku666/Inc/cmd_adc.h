#ifndef CMD_ADC_H_
#define CMD_ADC_H_

#include <stdint.h>

/**
 * @brief 解析 ADC 指令 ("adc")
 * @return 1 如果匹配成功并切换状态，0 否则
 */
uint8_t ADC_Parse(char* cmd);

/**
 * @brief ADC 轮询逻辑，处理 5Hz (200ms) 打印频率
 * @param now 当前系统毫秒数 (HAL_GetTick)
 */
void ADC_Update(uint32_t now);

#endif /* CMD_ADC_H_ */
