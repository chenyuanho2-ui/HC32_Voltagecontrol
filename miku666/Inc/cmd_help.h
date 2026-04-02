#ifndef CMD_HELP_H_
#define CMD_HELP_H_

#include <stdint.h>

/**
 * @brief 解析帮助指令 "instruction"
 * @param cmd 输入的指令字符串
 * @return 1 如果匹配并处理成功，0 否则
 */
uint8_t Help_Parse(char* cmd);

#endif /* CMD_HELP_H_ */
