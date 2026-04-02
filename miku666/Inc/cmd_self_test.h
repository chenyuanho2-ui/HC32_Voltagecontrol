/* cmd_self_test.h */
#ifndef __CMD_SELF_TEST_H
#define __CMD_SELF_TEST_H

#include <stdint.h>

/**
 * @brief 执行系统自检流程
 * @note 包含 1s 蜂鸣器提示、20/60/100 三点 ADC 采样（共 4.5s）
 */
void SelfTest_Run(void);

/**
 * @brief 解析自检相关指令
 * @param cmd 输入字符串
 * @return 1 表示匹配并处理了指令，0 表示未匹配
 */
int SelfTest_Parse(char* cmd);

#endif
