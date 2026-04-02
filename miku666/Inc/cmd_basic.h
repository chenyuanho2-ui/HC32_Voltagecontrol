#ifndef CMD_BASIC_H_
#define CMD_BASIC_H_

#include <stdint.h>

/**
 * @brief 解析基础指令（数值、s、p）
 * @return 1 如果匹配并处理成功，0 否则
 */
uint8_t Basic_Parse(char* cmd);

#endif
