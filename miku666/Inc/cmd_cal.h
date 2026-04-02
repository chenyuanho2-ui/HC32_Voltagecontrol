#ifndef CMD_CAL_H_
#define CMD_CAL_H_

#include <stdint.h>

/**
 * @brief 启动在线校准流程
 */
void Cal_Start(void);

/**
 * @brief 处理校准模式下的数值输入（指令值与测量值交替记录）
 */
void Cal_Process(char* cmd);

/**
 * @brief 结束校准并应用数据
 */
void Cal_End(void);

#endif
