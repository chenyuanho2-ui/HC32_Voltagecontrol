#ifndef CMD_RAMP_H_
#define CMD_RAMP_H_

#include <stdint.h>

/**
 * @brief 解析渐变指令
 * @return 1 如果匹配并处理成功，0 否则
 */
uint8_t Ramp_Parse(char* cmd);

/**
 * @brief 渐变逻辑轮询，更新当前速度
 */
void Ramp_Update(uint32_t now);

/**
 * @brief 停止当前渐变任务
 */
void Ramp_Stop(void);

#endif
