/**
 * @file cmd_buzzer.h
 * @brief 蜂鸣器控制模块头文件
 */

#ifndef CMD_BUZZER_H
#define CMD_BUZZER_H

#include <stdint.h>

/**
 * @brief 初始化蜂鸣器GPIO引脚
 */
void Buzzer_Init(void);

/**
 * @brief 解析蜂鸣器控制指令
 * @param cmd 输入的命令字符串
 * @return 1如果成功解析并执行，0否则
 */
uint8_t Buzzer_Parse(char* cmd);

/**
 * @brief 更新蜂鸣器定时任务（需在主循环中调用）
 * @param now 当前系统时间戳
 */
void Buzzer_Update(uint32_t now);

/**
 * @brief 设置蜂鸣器状态
 * @param state 1为开启，0为关闭
 */
void Buzzer_SetState(uint8_t state);

/**
 * @brief 获取当前蜂鸣器状态
 * @return 1为开启，0为关闭
 */
uint8_t Buzzer_GetState(void);

/**
 * @brief 检查是否有定时蜂鸣任务在运行
 * @return 1为有定时任务，0为无
 */
uint8_t Buzzer_IsTimerActive(void);

#endif // CMD_BUZZER_H
