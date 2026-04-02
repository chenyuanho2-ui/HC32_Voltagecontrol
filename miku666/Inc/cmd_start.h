#ifndef CMD_START_H_
#define CMD_START_H_

#include <stdint.h>

/**
 * @brief 解析启停指令 (on, off)
 */
uint8_t Start_Parse(char* cmd);

/**
 * @brief 初始化启停引脚 (PB13)
 */
void Start_Init(void);

#endif
