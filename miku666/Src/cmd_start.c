/**
 * @file cmd_start.c
 * @brief 设备启停控制模块
 * * 职责：
 * 1. 管理 PB13 引脚电平：低电平启动 (START)，高电平停止 (STOP)。
 * 2. 解析 'on' ：使引脚输出低电平以启动设备。
 * 3. 解析 'off'：使引脚输出高电平以停止设备。
 */

#include "cmd_start.h"
#include "instruction_manager.h"
#include <stdio.h>

void Start_Init(void) {
    Miku_Motor_SetStop(1u);
}

uint8_t Start_Parse(char* cmd) {
    if (custom_stricmp(cmd, "on") == 0) {
        Miku_Motor_SetStop(0u);
        printf(">> System: ON (Started)\r\n");
        return 1;
    }
    else if (custom_stricmp(cmd, "off") == 0) {
        Miku_Motor_SetStop(1u);
        printf(">> System: OFF (Stopped)\r\n");
        return 1;
    }
    return 0;
}
