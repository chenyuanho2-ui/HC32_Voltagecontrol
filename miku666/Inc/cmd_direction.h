#ifndef CMD_DIRECTION_H_
#define CMD_DIRECTION_H_

#include <stdint.h>

/**
 * @brief 解析方向指令 (ch0, ch1, ch)
 */
uint8_t Direction_Parse(char* cmd);

/**
 * @brief 初始化方向引脚 (PB12)
 */
void Direction_Init(void);

#endif
