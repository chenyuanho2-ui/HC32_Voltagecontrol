#ifndef CMD_TIMER_H_
#define CMD_TIMER_H_

#include <stdint.h>

/**
 * @brief 解析定时指令
 * @return 1 如果匹配并处理成功，0 否则
 */
uint8_t Timer_Parse(char* cmd);

/**
 * @brief 定时器轮询，判断是否到达停止时间
 */
void Timer_Update(uint32_t now);

/**
 * @brief 停止当前定时任务
 */
void Timer_Stop(void);

#endif
